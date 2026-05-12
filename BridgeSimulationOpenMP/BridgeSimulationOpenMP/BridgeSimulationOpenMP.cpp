//UNM CS 533
// Joseph Salazar
// Diego Ornelas

#include "BridgeSim.hpp"
#include <iostream>
#include <cstdlib>

/**
 * @brief Entry point for the Bridge Simulation OpenMP application.
 *
 * This program performs a computational blast pressure simulation across
 * a bridge surface using OpenMP-based parallel processing. The simulation
 * models the propagation of blast wave pressure from a detonation point
 * to individual bridge tiles over time.
 *
 * User-defined simulation parameters are provided through command line
 * arguments, including bridge dimensions, detonation location, thread
 * count, and optional binary file output generation.
 *
 * The BridgeSim class is responsible for:
 * - Allocating and managing simulation memory
 * - Precomputing blast-related physical values
 * - Executing time-stepped pressure calculations
 * - Parallelizing computations using OpenMP
 * - Optionally exporting pressure data to a binary file
 *
 * Expected command line arguments:
 * 1. Bridge length (feet)
 * 2. Bridge width (feet)
 * 3. Detonation height (feet)
 * 4. Detonation X location (feet)
 * 5. Detonation Y location (feet)
 * 6. Number of OpenMP threads
 * 7. Binary output flag (0 = false, 1 = true)
 *
 * Example:
 * ./BridgeSim 200 40 25 100 20 8 1
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 *
 * @return int Returns 0 on successful execution and nonzero on failure.
 */

int main(int argc, char* argv[])
{
    if (argc != 8) {
        std::cerr << "Usage:\n"
            << argv[0] << " <length_ft> <width_ft> <det_height_ft> "
            << "<det_x_ft> <det_y_ft> <threads> <trials>\n";
        return 1;
    }

    int bridge_length_ft = std::atoi(argv[1]);
    int bridge_width_ft = std::atoi(argv[2]);
    int det_height_ft = std::atoi(argv[3]);
    int det_x_ft = std::atoi(argv[4]);
    int det_y_ft = std::atoi(argv[5]);
    int thread_amount = std::atoi(argv[6]);
    bool produceBinaryFile = std::atoi(argv[7]) == 1;
    
    BridgeSim sim(
        bridge_length_ft,
        bridge_width_ft,
        det_height_ft,
        det_x_ft,
        det_y_ft,
        thread_amount,
        produceBinaryFile
    );
    
    sim.beginSimulation();

    return 0;
}
