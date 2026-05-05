#!/bin/bash

# --- CONSTANTS ---

srcDir="BridgeSimulationOpenMP/BridgeSimulationOpenMP"
srcFileMain="$srcDir/BridgeSimulationOpenMP.cpp"
srcFileUtil="$srcDir/BridgeSim.cpp"
dstDir="BridgeSimulationOpenMP"
dstFile="$dstDir/compiledSimulation"
threshold=35    # Optimal threshold for Agera UNMCS machine
logTemps=true
expReps=50      # Optimal repetitions value to balance LLN variability vs time remaining
visFile="visualization.py"
venvDir=".venv"

# --- SIMULATION PARAMETERS ---

simLength=900
simWidth=100
xMin=300
xMax=600
yMin=25
yMax=75
hMin=30
hMax=50

# --- COMMAND LINE ARGUMENTS ---

# Determine if simulation should save its pressure data
saveData=${1}

if [ -z "$saveData" ]; then
    saveData=false
fi

# Determine if script should launch visualization after experiment is done
launchVisTool=${2}

if [ -z "$launchVisTool" ]; then
    launchVisTool=false
fi

# --- HELPER FUNCTIONS ---

# Generates a CLI-based progress bar, visualizing experiment progress
# Usage: cli_prog_bar <current_position> <max_position> <descriptor_string>
cli_prog_bar() {
    local currPos=$1
    local maxPos=$2
    local descStr=$3

    # Determine how much bar should be filled based on progress made & terminal width
    local width=$((2 * $(tput cols) / 5))
    let "px = currPos * 1000 / maxPos"
    let "pr = px % 10"
    let "progress = px / 10"
    let "filled = progress * width / 100"
    let "empty = width - filled"

    # Build the progress bar string
    printf "\r$descStr Progress: ["
    printf "%${filled}s" | tr ' ' '#'
    printf "%${empty}s" | tr ' ' '-'
    printf "] $progress.$pr%%"
}

# Suspends all other processes currently running on the system
# If running as root, all processes suspended across the userbase
# If not running as root, only processes owned by this user suspended
suspendAll() {
    pidsList=()

    if [[ $EUID -eq 0 ]]; then
        # Root case: get all PIDs & exclude system processes (any PID where corresponding UID > 1000)
        pidsList=$(ps -eao pid,uid --no-headers | grep -vE "($$|$PPID)" | awk '$2 >= 1000 {print $1}')
    else
        # Non-root case: get only PIDs of this user, exclude this script and shell
        pidsList=$(ps -u $USER -o pid= | grep -vE "($$|$PPID)")
    fi

    # Iterate thru PIDs and send SIGSTOP to each
    for pid in $pidsList; do
        kill -STOP "$pid" 2> /dev/null
    done
}

# Resumes all processes which were previously stopped by a suspendAll call
resumeAll() {
    pidsList=()

    if [[ $EUID -eq 0 ]]; then
        # Root case: get all PIDs & exclude system processes (any PID where corresponding UID > 1000)
        pidsList=$(ps -eao pid,uid --no-headers | grep -vE "($$|$PPID)" | awk '$2 >= 1000 {print $1}')
    else
        # Non-root case: get only PIDs of this user, exclude this script and shell
        pidsList=$(ps -u $USER -o pid= | grep -vE "($$|$PPID)")
    fi

    # Iterate thru PIDs and send SIGCONT to each
    for pid in $pidsList; do
        kill -CONT "$pid" 2> /dev/null
    done
}

# --- PRE-EXPERIMENT SETUP TASKS ---

# Compile the simulation executable from source (abort if any errors)
g++ -std=c++20 -fopenmp "$srcFileMain" "$srcFileUtil" -o "$dstFile"

if [ $? -ne 0 ] || [ ! -f "$dstFile" ]; then
    echo "Error: unable to compile simulation source file. Aborting experiment..."
    exit 1
fi

# Determine max threads (CPU phys. cores) & create thread amounts iterable
maxThreads=$(nproc --all)
threadAmounts=(1 $(seq 2 2 $maxThreads))

# Set OpenMP env. variables to disable hyper-threading (only 1 thread per core)
export OMP_PROC_BIND=true
export OMP_PLACES=cores

# Find thermal zone which corresponds to the CPU
tempFile=""
for zone in /sys/class/thermal/thermal_zone*; do
    if [ -f "$zone/type" ]; then
        # Read device type & see if it matches expected value for CPU
        type=$(cat "$zone/type")
        if [ "$type" == "x86_pkg_temp" ]; then
            tempFile="$zone/temp"
            break
        fi
    fi
done

# Fallback if CPU thermal zone isn't found
if [ -z "$tempFile" ]; then
    if [ -f "/sys/class/thermal/thermal_zone0/temp" ]; then
        echo "Warning: 'x86_pkg_temp' temperature file not found. Using zone0 as CPU reading..."
        tempFile="/sys/class/thermal/thermal_zone0/temp"
    else
        echo "Error: unable to find any device temperature files. Aborting experiment..."
        exit 2
    fi
fi

# Set custom "EXP" env. variable: allows simulation to differentiate b/w script or user execution
export EXP="Script"

# Manually set the random seed for subsequent RNG in the experiment
RANDOM=1234

# Initialize log file w/ current timestamp to guarantee unique filename
logFile="Experiment_$(date +"%Y%m%d-%H%M%S").log"
echo "Experiment: running simulation with OpenMP multi-threading up to $maxThreads threads" > "$logFile"
echo "" >> "$logFile"

# Inform terminal via stdout that experiment execution is under way
echo "Simulation program compiled. Beginning experiment..."
startTime=$(date +%s)

# Hide cursor since script requires no input (trap to have cursor reappear if stopped)
tput civis

# Trap on script interrupt will 1) return cursor to shell 2) resume any stopped processes 3) remove EXP env. flag
trap "tput cnorm; resumeAll; export EXP=""; echo; exit 3" INT TERM

# --- MAIN EXPERIMENT LOOP ---

c=0

# Test simulation runtimes at all thread amounts in the iterable object
for n in "${threadAmounts[@]}"; do
    # Set thread amount via OpenMP env. variable
    export OMP_NUM_THREADS=$n

    # Inner loop: repeat sim multiple times at each thread amount to determine average runtimes
    for ((i=1; i<=$expReps; i++)); do
        # Update progress bar on CLI
        cli_prog_bar $c $((${#threadAmounts[@]} * $expReps)) "Experiment"

        # Report thread amount & iteration number to log file
        echo "Threads: $OMP_NUM_THREADS Iteration: $i" >> "$logFile"

        # Simulation execution hold: before performing the simulation, script will:
        # 1) Check CPU temp. & wait until the reading is below threshold temp.
        # 2) Check the # of running processes & wait until there less than 3 running
        # 3) Suspend other processes before sim. execution but only after temp is low. This 
        # generously allows other processes to run while script is waiting for lower temps anyway.
        while true; do
            # Temp readings at file are in millidegrees. must convert to degrees.
            currentTemp=$(($(cat "$tempFile") / 1000))
            currentProcs=$(ps -eo state | grep -c "^R")

            if [ "$currentTemp" -lt "$threshold" ]; then
                if [ "$currentProcs" -lt 3 ]; then
                    break
                else
                    suspendAll
                    sleep 1
                fi
            else
                sleep 1
            fi
        done

        # Determine random detonation location (dh, dx, dy) for this sim. run
        dh=$(( RANDOM % (hMax - hMin + 1) + hMin ))
        dx=$(( RANDOM % (xMax - xMin) + xMin ))
        dy=$(( RANDOM % (yMax - yMin) + yMin ))

        # Save sim. pressure data for first run (c == 0), otherwise skip
        s=0

        if $saveData; then
            if [ "$c" -eq 0 ]; then
                s=1
            fi
        fi
        
        # Execute the simulation command & determine the elapsed time
        # Sim. CL args: <length_ft> <width_ft> <det_height_ft> <det_x_ft> <det_y_ft> <threads> <save_data>
        errFile="Errors_$(date +"%Y%m%d-%H%M%S").log"
        runtime=$(eval "$dstFile $simLength $simWidth $dh $dx $dy $n $s" 2> "$errFile")

        # Check for any simulation errors & abort if one occurred
        if [ $? -ne 0 ]; then
            echo ""
            echo "Error: simulation exited with status 1. Output saved to: $errFile"
            tput cnorm
            resumeAll
            export EXP=""
            exit 6
        else
            rm "$errFile"
        fi

        # Report the recorded elapsed time to the log file
        echo "Elapsed Simulation Time (s): $runtime" >> "$logFile"

        # Report CPU temp at end of sim run, if temp. logging is enabled
        if $logTemps; then
            finalTemp=$(($(cat "$tempFile") / 1000))
            echo "Final CPU Temp (°C): $finalTemp" >> "$logFile"
        fi

        # Resume other processes & increment run counter used for progress bar updates
        resumeAll
        c=$((c + 1))
        echo "" >> "$logFile"
    done
done

# Add verification of full experiment run to log file (if this is missing, script was interrupted)
echo "Experiment Completion: YES" >> "$logFile"

# Inform terminal via stdout that experiment finished & where to find log file
cli_prog_bar 100 100 "Experiment"
echo ""
echo "...Experiment complete."
echo "Results saved: $logFile"

# --- POST-EXPERIMENT EXTRA TASKS ---

# 1. remove EXP env. variable now that experiment is complete
export EXP=""

# 2. Launch the Python visualization tool, which will visualize the blast wave
#    using pressure data stored into a contiguous binary file by the simulation.
#    NOTE: Python3 and it's environment manager, venv, must be installed by user.
#    If operating from a fresh Python installation with no packages installed,
#    any packages needed for the visualization will be automatically installed.

# Skip next steps if visualization after simulation feature diabled
if [ "$launchVisTool" == "false" ]; then
    tput cnorm
    exit 0
fi

echo "Starting the saved results visualization tool..."

# Check that python installed
if ! command -v python3 &> /dev/null; then
    echo "Error: Python3 is missing, and must be installed to run the visualization tool."
    tput cnorm
    exit 4
fi

# Try to launch the visualization tool now that Python hard dependency confirmed
python3 "$visFile" &> /dev/null

# Check if visualizaton tool failed to start due to ModuelNotFound error
# This means the necessary Python packages (soft dependencies) are missing
if [ $? -eq 1 ]; then
    echo "Warning: necessary Python packages are missing. Installing now..."

    # Check that Python virtual environment module installed
    if ! python3 -m venv --help &> /dev/null; then
        echo "Error: Python venv module is missing, and must be installed to manage Python packages."
        tput cnorm
        exit 5
    fi

    # Create & activate virtual Python environment in the git-ignored folder name
    cli_prog_bar 0 5 "Installation"
    python3 -m venv "$venvDir" --prompt omp
    source "$venvDir/bin/activate"
    cli_prog_bar 1 5 "Installation"

    # Use built-in Python package manager to install the necessary packages to virtual env.
    python -m pip install PySimpleGUI==4.60.5.1 --quiet
    cli_prog_bar 2 5 "Installation"
    python -m pip install numpy --quiet
    cli_prog_bar 3 5 "Installation"
    python -m pip install seaborn --quiet
    cli_prog_bar 4 5 "Installation"
    python -m pip install psutil --quiet
    cli_prog_bar 5 5 "Installation"
    echo ""

    # Restart visualization tool w/ installed packages, then deactivate virtual env. after
    echo "...Installation of Python packages complete. Retrying visualization now."
    python3 "$visFile" &> /dev/null
    deactivate
fi

tput cnorm
exit 0
