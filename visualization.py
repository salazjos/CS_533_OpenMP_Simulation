# Bridge Detonation Visualization via PySimpleGUI & Seaborn

import os
import PySimpleGUI as gui
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

MAX_PSI = 1000.0
DATA_FNAME = "BridgeSimulationOpenMP/mockData.npz"

# --- Data Generation ---
# Mock data currently produced by a different Python script (mockSim.py)
# The data is read in from the compressed Numpy file created by the other script
# TODO: use HighFive data-loading module instead, for interoperability w/ C++
def load_sim_data():
    if not os.path.exists(DATA_FNAME):
        print("Error: simulation results are missing. Ensure the simulation has completed & saved its data. Exiting...")
        exit(1)

    with np.load(DATA_FNAME) as simData:
        return simData['arr_0']

# Shape: (Time, X, Y)
DATA = load_sim_data()
TIMESTEPS, X, Y = DATA.shape
X_TICKS = 50
Y_TICKS = 20

# --- Helper Functions ---
def draw_figure(canvas, figure):
    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side='top', fill='both', expand=1)
    return figure_canvas_agg

def update_heatmap(t, ax, canvas_agg):
    ax.cla()  # Clear current axes
    # Custom Palette: White to Dark Red (TODO: change to custom red ranges)
    cmap = sns.light_palette("red", as_cmap=True)
    
    # Create the pressure wave animation as a Seaborn heatmap
    sns.heatmap(DATA[t], ax=ax, cmap=cmap, cbar=False, vmin=0, vmax=MAX_PSI, 
                xticklabels=X_TICKS, yticklabels=Y_TICKS, square=True, rasterized=True)
    
    # Update current timestep in heatmap title & invert y-axis, then draw on canvas
    ax.set_title(f"Pressure Wave Animation - Time Step: {t}ms")
    ax.invert_yaxis()
    canvas_agg.draw()

# --- GUI Layout ---
layout = [
    [gui.Text("Structural Pressure Wave Simulation - Visualization Tool", font=("Helvetica", 27))],
    [gui.HorizontalSeparator()],
    [
        gui.Text("Time Step (ms):", font=("Helvetica", 12)), 
        gui.Input(key='-TIME_INPUT-', size=(5, 1), enable_events=True, 
                 default_text="0", font=("Helvetica", 12)),
        gui.Text(f"/ {TIMESTEPS - 1}", font=("Helvetica", 12)),
        gui.VerticalSeparator(),
        gui.Button("Play", font=("Helvetica", 12)), 
        gui.Button("Pause", font=("Helvetica", 12)),
        gui.Button("Step 1ms", font=("Helvetica", 12)),
        gui.VerticalSeparator(),
        gui.Text("Playback Speed (Steps/s):", font=("Helvetica", 12)),
        gui.Slider(range=(0.1, 20.0), default_value=1.0, resolution=0.1, 
                  orientation='h', key='-SPEED-', font=("Helvetica", 12))
    ],
    [gui.Canvas(key='-CANVAS-', size=(1000, 200))]
]

# --- Main Function ---
def run_visualization():
    window = gui.Window("Structural Simulation", layout, finalize=True)

    # --- Initialize Matplotlib Figure ---
    fig, ax = plt.subplots(figsize=(10, 2))
    canvas_elem = window['-CANVAS-'].TKCanvas
    canvas_agg = draw_figure(canvas_elem, fig)
    update_heatmap(0, ax, canvas_agg)

    # --- State Variables ---
    current_time = 0
    is_playing = False

    # --- Event Loop ---
    while True:
        # Determine speed from Slider first (default to 1 if values empty on first run)
        speed = values['-SPEED-'] if 'values' in locals() and values['-SPEED-'] else 1.0

        # Adjust window timeout based on the computed speed from Slider value
        timeout = 1000 // speed if is_playing else None
        event, values = window.read(timeout=timeout)

        # Process primary events for play, pause, step & exit
        if event in (gui.WIN_CLOSED, 'Exit'):
            break

        if event == "Play":
            is_playing = True
        
        if event == "Pause":
            is_playing = False

        if event == "Step 1ms":
            if not is_playing and current_time < TIMESTEPS - 1:
                current_time += 1
                window['-TIME_INPUT-'].update(current_time)
                update_heatmap(current_time, ax, canvas_agg)

        # Secondary event: manual override of current time step
        if event == '-TIME_INPUT-':
            try:
                val = int(values['-TIME_INPUT-'])
                if 0 <= val < TIMESTEPS:
                    current_time = val
                    update_heatmap(current_time, ax, canvas_agg)
            except ValueError:
                pass

        # Secondary event: animation of heatmap over tie
        if event == "__TIMEOUT__":
            if is_playing and current_time < TIMESTEPS - 1:
                current_time += 1
            else:
                is_playing = False  # auto-pause once end of animation reached
            
            window['-TIME_INPUT-'].update(current_time)
            update_heatmap(current_time, ax, canvas_agg)

    window.close()

if __name__ == "__main__":
    run_visualization()



# NOTE: to get started with Python & run this file, use the following commands:
#   sudo apt install python3-full python3-venv
#   cd ~/
#   python -m venv base
#   source base/bin/activate
#   python -m pip install PySimpleGUI==4.60.5.1 numpy seaborn
#
# Now, you should have installed Python & created a virtual environment with
# the necessary modules to run this file (with 'python visualization.py').
# To automatically load this environment, add it to your ~/.bashrc file:
#   echo "source ~/base/bin/activate" >> ~/.bashrc

