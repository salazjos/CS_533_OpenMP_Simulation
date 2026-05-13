# Bridge Detonation Visualization via PySimpleGUI, Matplotlib & Seaborn
# UNM CS 533
# Diego Ornelas
# Joseph Salazar

# --- Python Dependencies ---

import os, psutil, time
import PySimpleGUI as gui
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

# --- Program Constants ---

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
    """
    Loads simulation pressure data from the raw binary file.

    Returns:
        SimData: Numpy 3D array containing all pressure data across all timesteps.
    """

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
    """
    Backup if the binary file is too large- loads the binary file timestep by timestep to reduce RAM usage.

    Parameters:
        numFrames (int): total number of timesteps/frames to load individually from the file.

    Returns:
        finalData: Numpy 3D array containing all pressure data across all timesteps.
    """

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
    """
    Converts a 3D pressure matrix from square-inch resolution to square-foot resolution.
    Conversion is performed by an average-pool of every 144 tiles into a single tile.
    After the pooling is completed, all tiles are rescaled to the original pressure range.

    Parameters:
        data_sq_in: Numpy 3D array of all pressure data in square-inch resolution.
        numFrames (int): total number of timesteps/frames to load individually from the file.

    Returns:
        data_sq_ft: Numpy 3D array of all pressure data in square-foot resolution.
    """

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
    """
    Backup if file size too large- each frame must have its resolution converted before storing on the heap.
    Square-foot frames require less space than square-inch frames, allowing RAM usage to be spared.

    Parameters:
        frame_sq_in: Numpy 2D array of the current pressure frame in square-inch resolution.

    Returns:
        frame_sq_ft: Numpy 2D array of the current pressure frame in square-foot resolution.
        maxPSI (float): maximum PSI value observed in the frame before conversion.
    """

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

# Custom Palette-   Light Grey: bridge background/no damage
#                   White: minimal damage
#                   Yellow: light damage
#                   Orange: moderate damage
#                   Red: heavy damage
#                   Dark Red: extreme damage
CMAP = sns.blend_palette(["#ffffff", "#fdb915", "#ffa500", "#f8481c", "#ff0000", "#840000"], 
                         n_colors=1000, as_cmap=True)
CMAP.set_under("#f0f0f0")

def setup_figure(canvas, figure):
    """
    Links the Matplotlib figure and canvas elements to the underlying TKinter widget in PySimpleGUI

    Parameters:
        canvas: the canvas element where the heatmap is drawn
        figure: the Matplotlib figure elemnt containing the canvas
    
    Returns:
        Figure-canvas aggregate created by TKinter.
    """

    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side='top', fill='both', expand=1)
    return figure_canvas_agg

def setup_heatmap(ax, canvas):
    """
    Initializes the heatmap and sets up the title, axis ticks and labels.

    Parameters:
        ax: Matplotlib axis object present in the rendered figure.
        canvas: canvas element rendered via the TKinter widget.

    Returns:
        ims: IMShow heatmap object which renders the animation.
    """
    ax.cla()  # Clear current axes (will also clear the heatmap too)
    
    # Create the pressure wave animation as an IMShow heatmap
    ims = ax.imshow(DATA[0], cmap=CMAP, vmin=0.01, vmax=MAX_PSI, interpolation="nearest")
    
    # Set the current time title and the custom x & y ticks/labels, then render
    ax.set_title("Pressure Wave Animation - Time Since Detonation: 0ms\n")
    ax.set_xticks(np.linspace(0, X, 19, dtype=np.int32))
    ax.set_xticklabels(np.linspace(0, X, 19, dtype=np.int32), rotation=0)
    ax.set_yticks(np.linspace(0, Y, 6, dtype=np.int32))
    ax.set_yticklabels(np.linspace(0, Y, 6, dtype=np.int32))
    ax.set_xlabel("\nBridge Length (ft)")
    ax.set_ylabel("Bridge Width (ft)\n")
    ax.invert_yaxis()
    canvas.draw()

    # Return the IMShow heatmap object for future updates
    return ims

def update_heatmap(t, ax, hmap, canvas):
    """
    Updates the heatmap animation and title for the given current timestep.

    Parameters:
        t (int): current timestep to index the pressure data required for rendering.
        ax: Matplotlib axis object present in the rendered figure.
        hmap: heatmap object used to render the animation onto the canvas.
        canvas: canvas element rendered via the TKinter widget.
    """

    # Load the next timestep by simply swapping the heatmap's data
    hmap.set_data(DATA[t])

    # Update the current time display in the heatmap axis title
    t_ms = int(t * FRAME_INTERVAL) if FRAME_INTERVAL >= 1 else np.round(t * FRAME_INTERVAL, 1)
    ax.set_title(f"Pressure Wave Animation - Time Since Detonation: {t_ms}ms\n")

    # Effeciently render the updates onto the canvas
    canvas.draw_idle()

def update_peakPSIreadout(window, timestep):
    """
    Updates the peak pressure observed value & location readouts for the curent timestep.

    Parameters:
        window: PySimpleGUI window element which contains all rendered text elements.
        timestep (int): current timestep of the displayed animation.
    """

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
    window["-WHERE-"].update(f"Where: {peakLoc}\t")

def time_ms():
    """
    Current time since the epochs, in milliseconds. Has decimal precision.
    """

    return time.time() * 1000

# --- GUI Layout ---
layout = [
    [gui.Text("Explosion Pressure Wave Simulation - Visualization Tool", font=("Helvetica", 24))],
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
        gui.Slider(range=(1.0, 20.0), default_value=1.0, resolution=0.5, 
                  orientation='h', key='-SPEED-', font=("Helvetica", 12))
    ],
    [gui.Canvas(key='-CANVAS-', size=(1000, 300), expand_x=True, expand_y=True)],
    [
        gui.Text("Peak Pressure Observed (PSI): 0.0\t", font=("Helvetica", 12), key="-MAX-"),
        gui.VerticalSeparator(),
        gui.Text("Where: N/A\t", font=("Helvetica", 12), key="-WHERE-")
    ]
]

# --- Main Function ---
def run_visualization():
    """
    Initializes the GUI window & all its elements, then starts the GUI event loop.
    The window is rendered as fast as possible- usually matches display FPS.
    Any events- button press, slider/text input- are processed when window read returns.
    Delta-time logic is used to render the correct animation and speed when playing.
    """

    # Initialize main GUI window w/ all layout elements- resizing enabled on window
    window = gui.Window("Pressure Wave Simulation", layout, resizable=True, finalize=True)

    # Enable resize events & forward to event flag "Resize" for later handling
    window.bind('<Configure>', "Resize")

    # Initialize Matplotlib figure, canvas & heatmap used to render animation
    fig, ax = plt.subplots(figsize=(10, 3), layout="constrained")
    canvas_elem = window['-CANVAS-'].TKCanvas
    canvas_agg = setup_figure(canvas_elem, fig)
    hmap = setup_heatmap(ax, canvas_agg)

    # State variables to determine current GUI status
    current_time = 0
    is_playing = False
    lastFrame = time_ms()
    animTimeout = 0
    lastWidth, _ = window["-CANVAS-"].get_size()

    # Event loop
    while True:
        # Determine speed from Slider first (default to 1 if values empty on first run)
        speed = values['-SPEED-'] if 'values' in locals() and values['-SPEED-'] else 1.0

        # Determine timeout value between frames based on speed from Slider value
        actualTimeout = 1000 / speed
        
        # Use non-blocking window read to get fastest possible animation
        event, values = window.read(timeout=0 if is_playing else None)

        # Process primary events for play, pause, step & exit
        if event in (gui.WIN_CLOSED, 'Exit'):
            break

        if event == "Play":
            is_playing = True
            lastFrame = time_ms()
            timeoutElapsed = time_ms() - lastFrame
            animTimeout = max(0.0, actualTimeout - timeoutElapsed)
        
        if event == "Pause":
            is_playing = False

        if event == "Step Fwd":
            if not is_playing and current_time < TIMESTEPS - 1:
                current_time += 1
                window['-TIME_INPUT-'].update(current_time)
                update_heatmap(current_time, ax, hmap, canvas_agg)
                update_peakPSIreadout(window, current_time)

        # Secondary event: manual override of current time step
        if event == '-TIME_INPUT-':
            try:
                val = int(values['-TIME_INPUT-'])
                if 0 <= val < TIMESTEPS:
                    current_time = val
                    update_heatmap(current_time, ax, hmap, canvas_agg)
                    update_peakPSIreadout(window, current_time)
            except ValueError:
                pass

        # Secondary event: window resize. Canvas auto resizes, simply must match figure size & redraw.
        if event == "Resize":
            c_width, _ = window["-CANVAS-"].get_size()

            # Debounce: only update after threshold exceed to prevent laggy GUI
            if c_width > 0 and np.abs(c_width - lastWidth) >= 10:
                c_height = int(c_width * 3) // 10
                window["-CANVAS-"].set_size((c_width, c_height))
                fig_width = c_width / 100
                fig.set_size_inches(fig_width, (fig_width * 3) / 10)
                update_heatmap(current_time, ax, hmap, canvas_agg)
                lastWidth = c_width

        # Tertiary "event": render next frame of the animation if enough time has passed
        if is_playing and (time_ms() - lastFrame) >= animTimeout:
            if current_time < (TIMESTEPS - 1):
                current_time += 1
            else:
                is_playing = False  # auto-pause once end of animation reached
            
            lastFrame = time_ms()
            window['-TIME_INPUT-'].update(current_time)
            update_heatmap(current_time, ax, hmap, canvas_agg)
            update_peakPSIreadout(window, current_time)

            # Update timeout value once frame finishes rendering
            timeoutElapsed = time_ms() - lastFrame
            animTimeout = max(0.0, actualTimeout - timeoutElapsed)

    window.close()


# --- Program Entrypoint ---
if __name__ == "__main__":
    run_visualization()


