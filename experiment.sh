#!/bin/bash

# TODO: change source code filename & idle CPU threshold temp. below
srcFile="bridgeSim.cpp"
threshold=40
logTemps=true

# Compile the simulation executable from source (abort if any errors)
g++ -std=c++20 -fopenmp -o bridge_sim "$srcFile"

if [ $? -ne 0 ] || [ ! -f "bridge_sim" ]; then
    echo "[$(date +"%D %T")] Error: unable to compile simulation source file. Aborting experiment..."
    exit 2
fi

# Determine max threads (CPU phys. cores) & create thread amounts iterable
max=$(nproc --all)
threadAmounts=(1 $(seq 2 2 $max))

# Set OpenMP env. variables to disable hyper-threading (only 1 thread per core)
export OMP_PROC_BIND=true
export OMP_PLACES=cores

# Initialize log file w/ current timestamp to guarantee unique filename
logFile="Experiment_$(date +"%Y%m%d-%H%M%S").log"
echo "Experiment: running simulation with OpenMP multi-threading up to $max threads" > "$logFile"
echo "Note: all timestamps are in seconds since Epoch, with nanosecond floating-point precision" >> "$logFile"
echo "" >> "$logFile"

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
        echo "[$(date +"%D %T")] Warning: 'x86_pkg_temp' temperature file not found. Using zone0 as CPU reading..."
        tempFile="/sys/class/thermal/thermal_zone0/temp"
    else
        echo "[$(date +"%D %T")] Error: unable to find any device temperature files. Aborting experiment..."
        exit 1
    fi
fi

# Inform terminal via stdout that experiment execution is under way
echo "[$(date +"%D %T")] Experiment in progress..."

# Main experiment loop: test simulation runtimes at all thread amounts
for n in "${threadAmounts[@]}"; do
    # Set thread amount via OpenMP env. variable
    export OMP_NUM_THREADS=$n

    # Inner loop: repeat sim multiple times at each thread amount to determine average runtimes
    for ((i=1; i<=20; i++)); do
        # Report thread amount & iteration number to log file
        echo "Threads: $OMP_NUM_THREADS Iteration: $i" >> "$logFile"

        # Check CPU temp. & wait until threshold temp. reached
        while true; do
            # Temp readings at file are in millidegrees. must convert to degrees.
            currentTemp=$(($(cat "$tempFile") / 1000))
            if [ "$currentTemp" -lt "$threshold" ]; then
                break
            else
                sleep 1
            fi
        done

        # TODO: suspend all other user processes on the system
        
        # Execute the simulation command & redirect output to log file
        # Simulation itself will report start & end times from omp_get_wtime
        ./bridge_sim >> "$logFile"

        # Report CPU temp at end of sim run, if temp. logging is enabled
        if $logTemps; then
            finalTemp=$(($(cat "$tempFile") / 1000))
            echo "Final CPU Temp (°C): $finalTemp" >> "$logFile"
        fi

        echo "" >> "$logFile"
    done
done

# Add verification of full experiment run to log file (if this is missing, script was interrupted)
echo "Experiment Completion: YES" >> "$logFile"

# Inform terminal via stdout that experiment finished & where to find log file
echo "[$(date +"%D %T")] ...Experiment complete."
echo "[$(date +"%D %T")] Results saved: $logFile"

exit 0
