// Bridge Simulation OpenMP

#include <omp.h>
#include <string>
#include <iostream>
#include <memory>
#include <cmath>

constexpr int Bridge_Length_Feet = 900; 
constexpr int Bridge_Width_Feet = 100;
constexpr int Array_Size = (Bridge_Length_Feet * 12) * (Bridge_Width_Feet * 12);
constexpr int Detonation_Height_Inches = 30 * 12;
constexpr int Detonation_X_Location = 800 * 12;
constexpr int Detonation_Y_Location = 50 * 12;

std::unique_ptr<float[]> bridge_tile_distance_from_detonation_array;
std::unique_ptr<float[]> blast_to_tile_theta_array;
std::unique_ptr<float[]> blast_tile_ground_distance_to_detonation_array;


void allocateArray(std::unique_ptr<float[]>& ptr, int size) {
    ptr = std::make_unique<float[]>(size);
}

void computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& arr, int size)
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

        arr[i] = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
    }
}

void computeGroundDistanceDetonationToTiles(std::unique_ptr<float[]>& arr, int size) {
    int Width = Bridge_Width_Feet * 12;

    #pragma omp parallel for
    for (int i = 0; i < size; ++i)
    {
        int x = i % Width;
        int y = i / Width;

        float dx = x - Detonation_X_Location;
        float dy = y - Detonation_Y_Location;

        arr[i] = sqrtf(dx * dx + dy * dy);
    }
}

void computeAngleFromDetenationToTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& angleArr, int size) {

    const float dz = Detonation_Height_Inches;

    #pragma omp parallel for
    for (int i = 0; i < size; ++i){
        angleArr[i] = atan2f(dz, distanceArr[i]);
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
    int threads = 1;
    omp_set_num_threads(threads);

    double start_time = omp_get_wtime();

    allocateArray(bridge_tile_distance_from_detonation_array, Array_Size);

    allocateArray(blast_to_tile_theta_array, Array_Size);

    allocateArray(blast_tile_ground_distance_to_detonation_array, Array_Size);

    

    //TODO: Surround all this with for-loop based on trial_amount.

    //This should be D1 from the diagram
    computeDistanceBlastDetonationToTiles(bridge_tile_distance_from_detonation_array, Array_Size);

    //This should be Z1 from the diagram
    computeGroundDistanceDetonationToTiles(blast_tile_ground_distance_to_detonation_array, Array_Size);

    //This should be A1 from the diagram
    computeAngleFromDetenationToTile(bridge_tile_distance_from_detonation_array, blast_to_tile_theta_array, Array_Size);
   
        
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
