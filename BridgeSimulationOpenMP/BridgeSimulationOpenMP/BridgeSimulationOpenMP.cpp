// Bridge Simulation OpenMP
#include "BridgeSim.hpp"
#include <iostream>
#include <cstdlib>

/*
Command line arguments required (in order):
int - bridge legth (feet)
int - bridge width (feet)
int - detonation height (feet)
int - detonation x location (feet)
int - detonation y location (feet)
int - thread amount
int - trial amount
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
    int trial_amount = std::atoi(argv[7]);

    BridgeSim sim(
        bridge_length_ft,
        bridge_width_ft,
        det_height_ft,
        det_x_ft,
        det_y_ft,
        thread_amount,
        trial_amount
    );
    
    sim.beginSimulation();

    return 0;
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
