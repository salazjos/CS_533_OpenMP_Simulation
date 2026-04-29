# CS-533 Final Project: OpenMP Simulation

## Usage

- **Launch Experiment:** `chmod +x experiment.sh && ./experiment.sh`
   - Use `./experiment.sh true` to launch the visualization tool once experiment is complete.
- **Compile Simulation:** `g++ -std=c++20 -fopenmp BridgeSimulationOpenMP.cpp BridgeSim.cpp -o simulation`
   - Note: the experiment script will recompile the simulation automatically before running each experiment.
- **Execute Simulation:** `./simulation <bridge_length> <bridge_widtn> <detonation_height> <detonation_x> <detonation_y>`
- **Start Visualization Tool:** `source .venv/bin/activate && python visualization.py`
    - If .venv directory is missing, create the virtual environment: `python -m venv .venv --prompt <env_tag>`
    - If ModuleNotFound error, install the necessary Python packages: `python -m pip install PySimpleGUI==4.60.5.1 numpy seaborn --quiet`
    - Alternatively, start the visualization tool from the experiment script for the first run- the script will create the virtual environment & install all Python packages automatically
