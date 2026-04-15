#!/bin/bash

# --- CONSTANTS ---

srcFile="BridgeSimulationOpenMP/BridgeSimulationOpenMP/BridgeSimulationOpenMP.cpp"
dstFile="BridgeSimulationOpenMP/compiledSimulation"
threshold=40    # TODO: adjust idle CPU threshold temp once hardware is decided
logTemps=true
expReps=20      # TODO: adjust exp. repetitions, if result variance too high

# --- HELPER FUNCTIONS ---

# Generates a CLI-based progress bar, visualizing experiment progress
# Usage: cli_prog_bar <current_position> <max_position>
cli_prog_bar() {
    local currPos=$1
    local maxPos=$2

    # Determine how much bar should be filled based on progress made & terminal width
    local width=$(($(tput cols) / 4))
    let "progress = currPos * 100 / maxPos"
    let "filled = progress * width / 100"
    let "empty = width - filled"

    # Build the progress bar string
    printf "\rProgress: ["
    printf "%${filled}s" | tr ' ' '#'
    printf "%${empty}s" | tr ' ' '-'
    printf "] $progress%%"
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
g++ -std=c++20 -fopenmp -o "$dstFile" "$srcFile"

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

# Initialize log file w/ current timestamp to guarantee unique filename
logFile="Experiment_$(date +"%Y%m%d-%H%M%S").log"
echo "Experiment: running simulation with OpenMP multi-threading up to $maxThreads threads" > "$logFile"
echo "" >> "$logFile"

# Inform terminal via stdout that experiment execution is under way
echo "Simulation program compiled. Beginning experiment..."
startTime=$(date +%s)

# Hide cursor since script requires no input (trap to have cursor reappear if stopped)
tput civis

# Trap on script interruption will 1) return cursor to shell and 2) resume any stopped processes
trap "tput cnorm; resumeAll; echo; exit 3" INT TERM

# --- MAIN EXPERIMENT LOOP

c=0

# Test simulation runtimes at all thread amounts in the iterable object
for n in "${threadAmounts[@]}"; do
    # Set thread amount via OpenMP env. variable
    export OMP_NUM_THREADS=$n

    # Inner loop: repeat sim multiple times at each thread amount to determine average runtimes
    for ((i=1; i<=$expReps; i++)); do
        # Update progress bar on CLI
        cli_prog_bar $c $((${#threadAmounts[@]} * $expReps))

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
                fi
            else
                sleep 1
            fi
        done
        
        # Execute the simulation command & determine the elapsed time
        runtime=$(eval "$dstFile")

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
cli_prog_bar 100 100
echo ""
echo "...Experiment complete."
echo "Results saved: $logFile"
tput cnorm
exit 0
