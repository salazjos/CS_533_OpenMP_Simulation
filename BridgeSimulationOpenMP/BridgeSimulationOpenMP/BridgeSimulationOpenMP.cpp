// Bridge Simulation OpenMP

#include <omp.h>
#include <string>
#include <iostream>

constexpr int Bridge_Length_Feet = 900; 
constexpr int Bridge_Width_Feet = 100;
constexpr int Array_Size = (Bridge_Length_Feet * 12) * (Bridge_Width_Feet * 12);
constexpr int Detonation_Height_Inches = 120 * 12;
constexpr int Detonation_X_Location = 800 * 12;
constexpr int Detonation_Y_Location = 50 * 12;

std::unique_ptr<double[]> bridge_tile_distance;


void allocateArray(std::unique_ptr<double[]>& ptr, int size) {
    ptr = std::make_unique<double[]>(size);
}

void computeDistances(std::unique_ptr<double[]>& arr, int size)
{
    int Width = Bridge_Width_Feet * 12;
    #pragma omp parallel for
    for (int i = 0; i < size; ++i)
    {
        int x = i % Width;
        int y = i / Width;

        double dx = x - Detonation_X_Location;
        double dy = y - Detonation_Y_Location;
        double dz = Detonation_Height_Inches;

        arr[i] = std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}

int main(int argc, char* argv[])
{
    // 🔹 Get number of trials from bash
    /*if (argc < 2) {
        std::cerr << "Usage: ./BridgeSimulationOpenMP <number of trials>\n";
        return 1;
    }*/

    //TODO set the trial amount from the bash script.
    //int trail_amount = std::stoi(argv[1]);

    //TODO: for testing without the bash script, manually set the number of trials.
    int trial_amount = 1;

    // Get OpenMP thread count (set by bash)
    //int threads = omp_get_max_threads();

    //TODO: for testing without the bash script, manually set the number of threads. 
    int threads = 2;  
    omp_set_num_threads(threads);

    allocateArray(bridge_tile_distance, Array_Size);

    double start_time = omp_get_wtime();

    for(int i = 0; i < trial_amount; i++)
        computeDistances(bridge_tile_distance, Array_Size);
    double end_time = omp_get_wtime();

    double elapsed_time = end_time - start_time;

    // TODO: the bash script should see this. 
    // example: output=$(./bridgeSim)
    std::cout << elapsed_time << std::endl;

    //std::cout << "elapsed time: " << elapsed_time;
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
