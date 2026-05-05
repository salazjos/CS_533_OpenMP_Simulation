# Bridge Detonation Visualization via PySimpleGUI & Seaborn

import os, psutil
import PySimpleGUI as gui
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# Path to file containing binary pressure data created by sim. program
DATA_FNAME = "BridgeSimulationOpenMP/pressureData.bin"

# Constant bridge dimensions from CPP simulation program
X_IN = 900 * 12
Y_IN = 100 * 12
FRAME_INTERVAL = 1.0

# --- Data Generation ---
# The data is read in from a binary data file, of known size & dimensions
# NOTE: file is always in row-major order- that is, shape = (time, width, length)
def load_sim_data():
    if not os.path.exists(DATA_FNAME):
        print("Error: simulation results are missing. Ensure the simulation has completed & saved its data. Exiting...")
        exit(2)

    # Determine file size & number of timesteps (file size over total tiles times 4 bytes)
    DATA_FSIZE = os.path.getsize(DATA_FNAME)
    totalTimesteps = DATA_FSIZE // (X_IN * Y_IN * 4)

    # Load the full data file into Numpy array, reshape into 3D, convert entire array & return
    # NOTE: if too large for RAM size, file has to be loaded in frame by frame
    if DATA_FSIZE < psutil.virtual_memory().available:
        simData = np.fromfile(DATA_FNAME, dtype=np.float32)
        simData = np.reshape(simData, (totalTimesteps, Y_IN, X_IN))
        simData = convert_data_tile_size(simData, totalTimesteps)
        return simData, totalTimesteps
    else:
        print("Warning: large file size detected. Visualization may take longer than usual to start...")
        return load_data_in_frames(totalTimesteps), totalTimesteps

# Fallback if file size > RAM: map the filespace w/ Numpy memmap & load in frame by frame
# Each frame must be downscaled fron sq in to sq ft immediately to avoid overflow
def load_data_in_frames(numFrames):
    # Initialize final array (known size: (T, Y / 12, X / 12))
    totalData = np.zeros((numFrames, Y_IN // 12, X_IN // 12), dtype=np.float32)

    # Create memory-disk mapping using built-in Numpy method (shape: (T, X*Y))
    dataMap = np.memmap(DATA_FNAME, dtype=np.float32, mode="r", shape=(numFrames, X_IN * Y_IN))

    # Loop thru total timesteps & load frame for that timestep into memory
    maxPSI = 0
    Q1_index = numFrames // 4
    for t in range(numFrames):
        frame = dataMap[t, :]
        frame = np.reshape(frame, (Y_IN, X_IN))

        # Use frame-specific conversion for sq in -> sq ft
        frame, maxContender = convert_frame_tile_size(frame)

        # Update max PSI if new frame saw higher max (assume max occurs w/in 1st quartile of frames)
        if t < Q1_index:
            maxPSI = max(maxPSI, maxContender)

        # Add the frame into the final array @ current timestep
        totalData[t] = frame

    # Ensure converted data is scaled to same pressure value ranges as original data
    try:
        maxRescaled = np.max(totalData[:Q1_index, :, :])
        finalData = maxPSI * totalData / maxRescaled
    except ValueError:
        finalData = 144.0 * totalData

    return finalData

# --- Data Transformation ---
# NOTE: visualization tool cannot draw inch tiles without lagging the animation.
# Solution: loaded data from simulation will be converted from sq-inch to sq-foot tiles.
def convert_data_tile_size(data_sq_in, numFrames):
    # Determine new data dimensions & max PSI before rescaling
    x_ft = X_IN // 12
    y_ft = Y_IN // 12
    Q1_index = numFrames // 4
    maxPSI = np.max(data_sq_in[:Q1_index, :, :])

    # Reshape to separate each 12x12 block
    # New shape: (T, x_ft, 12, y_ft, 12)
    poolingArr = data_sq_in.reshape(numFrames, y_ft, 12, x_ft, 12)
    
    # Get avg pressure value over the 12x12 blocks (axes 2 and 4 of the pooling array)
    downscaledArr = np.average(poolingArr, axis=(2, 4))

    # Ensure converted data is scaled to same pressure value ranges as original data
    try:
        maxRescaled = np.max(downscaledArr[:Q1_index, :, :])
        data_sq_ft = maxPSI * downscaledArr / maxRescaled
    except ValueError:
        data_sq_ft = 144.0 * downscaledArr
    
    # Return the converted array
    return data_sq_ft

# Fallback if file size > RAM: convert each individual frame to sq ft before saving to data buffer
def convert_frame_tile_size(frame_sq_in):
    # Determine new frame dimensions & max PSI before rescaling
    x_ft = X_IN // 12
    y_ft = Y_IN // 12
    maxPSI = np.max(frame_sq_in)

    # Reshape to separate each 12x12 block
    # New shape: (x_ft, 12, y_ft, 12)
    poolingFrame = frame_sq_in.reshape(y_ft, 12, x_ft, 12)

    # Get avg pressure value over the 12x12 blocks (axes 1 and 3 of the pooling frame)
    frame_sq_ft = np.average(poolingFrame, axis=(1, 3))

    # Return the converted frame & max PSI observed (for rescaling later)
    return frame_sq_ft, maxPSI

# Use load data function once to get the pressure data & timesteps amount
DATA, TIMESTEPS = load_sim_data()
X = X_IN // 12
Y = Y_IN // 12

# Determine maximum pressure & center point of the detonation
# NOTE: Assume max PSI & center arrival occur w/in first quartile of the data
Q1_index = TIMESTEPS // 4
MAX_PSI = np.max(DATA[:Q1_index, :, :])
_, C_y, C_x = np.unravel_index(np.argmax(DATA[:Q1_index, :, :]), DATA.shape)

# --- GUI Functions ---
def draw_figure(canvas, figure):
    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side='top', fill='both', expand=1)
    return figure_canvas_agg

def update_heatmap(t, ax, canvas_agg):
    ax.cla()  # Clear current axes (will also clear the heatmap too)
    # Custom Palette: White @ 0.0 PSI, light-dark red otherwise up to Max PSI
    cmap = sns.blend_palette(["#ffcccc", "darkred"], n_colors=100, as_cmap=True)
    cmap.set_under("white")
    
    # Create the pressure wave animation as a Seaborn heatmap
    sns.heatmap(DATA[t], ax=ax, cmap=cmap, cbar=False, vmin=0.01, 
                vmax=MAX_PSI, square=True, rasterized=True)
    
    # Update current timestep in heatmap title & set custom x & y ticks/labels, then render
    t_ms = int(t * FRAME_INTERVAL) if FRAME_INTERVAL >= 1 else np.round(t * FRAME_INTERVAL, 1)
    ax.set_title(f"Pressure Wave Animation - Time Since Detonation: {t_ms}ms\n")
    ax.set_xticks(np.linspace(0, X, 19, dtype=np.int32))
    ax.set_xticklabels(np.linspace(0, X, 19, dtype=np.int32), rotation=0)
    ax.set_yticks(np.linspace(0, Y, 6, dtype=np.int32))
    ax.set_yticklabels(np.linspace(0, Y, 6, dtype=np.int32))
    ax.set_xlabel("\nBridge Length (ft)")
    ax.set_ylabel("Bridge Width (ft)\n")
    ax.invert_yaxis()
    canvas_agg.draw()

def update_peakPSIreadout(window, timestep):
    # Get peak pressure value for current timestep
    frame = DATA[timestep]
    peakPSI = np.max(frame)

    # Determine if peak pressure is at center or edge (or N/A if zero pressure)
    peakLoc = "N/A"
    if peakPSI > 0.0:
        y, x = np.unravel_index(np.argmax(frame), frame.shape)
        if x == C_x and y == C_y:
            peakLoc = str(f"Center ({x}ft by {y}ft)")
        else:
            r = np.sqrt(((C_x - x) ** 2) + ((C_y - y) ** 2))
            peakLoc = str(f"Edge (radius= {r:.1f}ft)")

    # Update peak pressure & location readouts in GUI
    window["-MAX-"].update(f"Peak Pressure Observed (PSI): {peakPSI:.1f}\t")
    window["-WHERE-"].update(f"Where: {peakLoc}")

# --- GUI Layout ---
layout = [
    [gui.Text("Explosion Pressure Wave Simulation - Visualization Tool", font=("Helvetica", 28))],
    [gui.HorizontalSeparator()],
    [
        gui.Text("Current Timestep:", font=("Helvetica", 12)), 
        gui.Input(key='-TIME_INPUT-', size=(5, 1), enable_events=True, 
                 default_text="0", font=("Helvetica", 12)),
        gui.Text(f"/ {TIMESTEPS - 1}", font=("Helvetica", 12)),
        gui.VerticalSeparator(),
        gui.Button("Play", font=("Helvetica", 12)), 
        gui.Button("Pause", font=("Helvetica", 12)),
        gui.Button("Step Fwd", font=("Helvetica", 12)),
        gui.VerticalSeparator(),
        gui.Text("Playback Speed (Steps/s):", font=("Helvetica", 12)),
        gui.Slider(range=(0.1, 10.0), default_value=1.0, resolution=0.1, 
                  orientation='h', key='-SPEED-', font=("Helvetica", 12))
    ],
    [gui.Canvas(key='-CANVAS-', size=(1000, 300))],
    [
        gui.Text("Peak Pressure Observed (PSI): 0.0\t", font=("Helvetica", 12), key="-MAX-"),
        gui.VerticalSeparator(),
        gui.Text("Where: N/A", font=("Helvetica", 12), key="-WHERE-")
    ]
]

# --- Main Function ---
def run_visualization():
    window = gui.Window("Pressure Wave Simulation", layout, finalize=True)

    # --- Initialize Matplotlib Figure ---
    fig, ax = plt.subplots(figsize=(10, 3))
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

        if event == "Step Fwd":
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
#   python -m pip install PySimpleGUI==4.60.5.1 numpy seaborn psutil
#
# Now, you should have installed Python & created a virtual environment with
# the necessary modules to run this file (with 'python visualization.py').
# To automatically load this environment, add it to your ~/.bashrc file:
#   echo "source ~/base/bin/activate" >> ~/.bashrc

