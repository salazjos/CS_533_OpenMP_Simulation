#!/bin/bash

# TODO: change executable filename & idle CPU threshold temp. below
program="sleep 0.01"
threshold=30.0

# Determine max threads (CPU phys. cores) & create thread amounts iterable
max=$(nproc --all)
threadAmounts=(1 $(seq 2 2 $max))

# Set OpenMP env. variables to disable hyper-threading (only 1 thread per core)
export OMP_PROC_BIND=true
export OMP_PLACES=cores

# Initialize log file w/ current timestamp to guarantee unique filename
logfile="Experiment_$(date +"%Y%m%d-%H%M%S").log"
echo "Experiment: running simulation with OpenMP multi-threading up to $max threads" > "$logfile"
echo "Note: all timestamps are in seconds since Epoch, with nanosecond floating-point precision" >> "$logfile"
echo "" >> "$logfile"

# Inform terminal via stdout that experiment execution is under way
echo "[$(date +"%D %T")] Experiment in progress..."

# Main experiment loop: test simulation runtimes at all thread amounts
for n in "${threadAmounts[@]}"; do
    # Set thread amount via OpenMP env. variable
    export OMP_NUM_THREADS=$n

    # Inner loop: repeat sim multiple times at each thread amount to determine average runtimes
    for ((i=1; i<=20; i++)); do
        # Report thread amount & iteration number to log file
        echo "Threads: $n Iteration: $i" >> "$logfile"

        # TODO: check CPU temp & hold until threshold temp reached
        # TODO: suspend all other user processes on the system
        
        # Execute the simulation command, while saving the start & end times
        start=$(date +"%s.%N")
        eval $program
        end=$(date +"%s.%N")

        # Report recorded start & end times to log file
        echo "Simulation Start: $start" >> "$logfile"
        echo "Simulation End: $end" >> "$logfile"
        echo "" >> "$logfile"
    done
done

# Add verification of full experiment run to log file (if this is missing, script was interrupted)
echo "Experiment Completion: YES" >> "$logfile"

# Inform terminal via stdout that experiment finished & where to find log file
echo "[$(date +"%D %T")] ...Experiment complete."
echo "[$(date +"%D %T")] Results saved: $logfile"

exit 0
