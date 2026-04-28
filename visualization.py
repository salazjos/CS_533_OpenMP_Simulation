# Bridge Detonation Visualization via PySimpleGUI & Seaborn

import os
import PySimpleGUI as gui
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# TODO: switch file destination to actual data path once real sim. complete
DATA_FNAME = "BridgeSimulationOpenMP/mockData.bin"

# Constant bridge dimensions from CPP simulation program
X_IN = 900 * 12
Y_IN = 100 * 12

# --- Data Generation ---
# Mock data currently produced by a mock CPP program (mockSim.cpp)
# The data is read in from a binary data file, of known size & dimensions
# NOTE: file is always in row-major order- that is, shape = (time, width, length)
def load_sim_data():
    if not os.path.exists(DATA_FNAME):
        print("Error: simulation results are missing. Ensure the simulation has completed & saved its data. Exiting...")
        exit(2)

    # Determine file size & number of timesteps (file size over total tiles times 4 bytes)
    DATA_FSIZE = os.path.getsize(DATA_FNAME)
    totalTimesteps = DATA_FSIZE // (X_IN * Y_IN * 4)

    # Load the data file into Numpy array, reshape into 3D, then return array & timesteps amount
    simData = np.fromfile(DATA_FNAME, dtype=np.float32)
    simData = np.reshape(simData, (totalTimesteps, Y_IN, X_IN))
    return simData, totalTimesteps

# Use load data function once to get the pressure data & timesteps amount
DATA, TIMESTEPS = load_sim_data()

# Determine max pressure before unit conversion to preserve pressure value range
MAX_PSI = np.max(DATA)

# --- Data Transformation ---
# NOTE: visualization tool cannot draw inch tiles without lagging the animation.
# Solution: loaded data from simulation will be converted from sq-inch to sq-foot tiles.
def convert_data_tile_size(data_sq_in):
    # Determine new data dimensions & initialize new Numpy array
    x_ft = X_IN // 12
    y_ft = Y_IN // 12

    # Reshape to separate each 12x12 block
    # New shape: (T, x_ft, 12, y_ft, 12)
    poolingArr = data_sq_in.reshape(TIMESTEPS, y_ft, 12, x_ft, 12)
    
    # Get avg pressure value over the 12x12 blocks (axes 2 and 4 of the pooling array)
    downscaledArr = np.average(poolingArr, axis=(2, 4))

    # Ensure converted data is scaled to same pressure value ranges as original data
    maxRescaled = np.max(downscaledArr)
    data_sq_ft = MAX_PSI * downscaledArr / maxRescaled
    
    # Return the converted array & its new spatial dimensions
    return data_sq_ft, x_ft, y_ft

# Use convert data function to get data in sq. ft. specific for animations
DATA, X, Y = convert_data_tile_size(DATA)

# Determine center point of the detonation (location of max value at t=1ms)
C_x, C_y = np.unravel_index(np.argmax(DATA[1]), DATA[1].shape)

# --- GUI Functions ---
def draw_figure(canvas, figure):
    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side='top', fill='both', expand=1)
    return figure_canvas_agg

def update_heatmap(t, ax, canvas_agg):
    ax.cla()  # Clear current axes (will also clear the heatmap too)
    # Custom Palette: White @ 0.0 PSI, light-dark red otherwise up to Max PSI
    cmap = sns.blend_palette(["#ffcccc", "darkred"], n_colors=10, as_cmap=True)
    cmap.set_under("white")
    
    # Create the pressure wave animation as a Seaborn heatmap
    sns.heatmap(DATA[t], ax=ax, cmap=cmap, cbar=False, vmin=0.1, 
                vmax=MAX_PSI, square=True, rasterized=True)
    
    # Update current timestep in heatmap title & set custom x & y ticks/labels, then render
    ax.set_title(f"Pressure Wave Animation - Time Step: {t}ms\n")
    ax.set_xticks(np.linspace(0, X, 19, dtype=np.int32))
    ax.set_xticklabels(np.linspace(0, X, 19, dtype=np.int32), rotation=0)
    ax.set_yticks(np.linspace(0, Y, 6, dtype=np.int32))
    ax.set_yticklabels(np.linspace(0, Y, 6, dtype=np.int32))
    ax.set_xlabel("Bridge Length (ft)")
    ax.set_ylabel("Bridge Width (ft)")
    ax.invert_yaxis()
    canvas_agg.draw()

def update_peakPSIreadout(window, timestep):
    # Get peak pressure value for current timestep
    frame = DATA[timestep]
    peakPSI = np.max(frame)

    # Determine if peak pressure is at center or edge (or N/A if zero pressure)
    peakLoc = "N/A"
    if peakPSI > 0.0:
        x, y = np.unravel_index(np.argmax(frame), frame.shape)
        if x == C_x and y == C_y:
            peakLoc = str(f"Center ({y}ft by {x}ft)")
        else:
            r = np.sqrt(((x - C_x) ** 2) + ((y - C_y) ** 2))
            peakLoc = str(f"Edge (radius= {r:.1f}ft)")

    # Update peak pressure & location readouts in GUI
    window["-MAX-"].update(f"Peak Pressure Observed (PSI): {peakPSI:.1f}\t")
    window["-WHERE-"].update(f"Where: {peakLoc}")

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
    [gui.Canvas(key='-CANVAS-', size=(1000, 200))],
    [
        gui.Text("Peak Pressure Observed (PSI): 0.0\t", font=("Helvetica", 12), key="-MAX-"),
        gui.VerticalSeparator(),
        gui.Text("Where: N/A", font=("Helvetica", 12), key="-WHERE-")
    ]
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
                update_peakPSIreadout(window, current_time)

        # Secondary event: manual override of current time step
        if event == '-TIME_INPUT-':
            try:
                val = int(values['-TIME_INPUT-'])
                if 0 <= val < TIMESTEPS:
                    current_time = val
                    update_heatmap(current_time, ax, canvas_agg)
                    update_peakPSIreadout(window, current_time)
            except ValueError:
                pass

        # Secondary event: animation of heatmap over time
        if event == "__TIMEOUT__":
            if is_playing and current_time < TIMESTEPS - 1:
                current_time += 1
            else:
                is_playing = False  # auto-pause once end of animation reached
            
            window['-TIME_INPUT-'].update(current_time)
            update_heatmap(current_time, ax, canvas_agg)
            update_peakPSIreadout(window, current_time)

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

