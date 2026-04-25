// Bridge Simulation OpenMP

#include <omp.h>
#include <string>
#include <iostream>
#include <memory>
#include <cmath>
#include <numbers>

constexpr int Bridge_Length_Feet = 900; 
constexpr int Bridge_Width_Feet = 100;
constexpr int Array_Size = (Bridge_Length_Feet * 12) * (Bridge_Width_Feet * 12);
constexpr int Detonation_Height_Inches = 30 * 12;
constexpr int Detonation_X_Location = 800 * 12;
constexpr int Detonation_Y_Location = 50 * 12;

std::unique_ptr<float[]> bridge_tile_distance_from_detonation_array;
std::unique_ptr<float[]> blast_to_tile_theta_array;
std::unique_ptr<float[]> blast_tile_ground_distance_to_detonation_array;
std::unique_ptr<float[]> blast_tile_basic_peak_pressure;
std::unique_ptr<float[]> impulse_value_per_tile;
std::unique_ptr<float[]> peak_pressure_per_tile;
std::unique_ptr<float[]> time_of_arrival_per_tile;
std::unique_ptr<float[]> load_duration_per_tile;

void allocateArray(std::unique_ptr<float[]>& ptr, int size) {
    ptr = std::make_unique<float[]>(size);
}

void computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& arr, int size)
{
    int Width = Bridge_Width_Feet * 12;
    #pragma omp parallel for default(none)
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

    #pragma omp parallel for default(none)
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

    #pragma omp parallel for default(none)
    for (int i = 0; i < size; ++i){
        float ratio = dz / distanceArr[i];
        ratio = fminf(1.0f, fmaxf(-1.0f, ratio));
        float theta = acosf(ratio);
        float degrees = theta * 180.0f / std::numbers::pi;
        angleArr[i] = degrees;
    }
}

void computeBasicPeakPressureForTiles(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr, int size) {

    const float Kg_Conversion_Rate = 2.205;
    const float Charge_Weight_Pounds = 1000.0f;
    const float W_Kg = Charge_Weight_Pounds / Kg_Conversion_Rate;

    #pragma omp parallel for default(none)
    for (int i = 0; i < size; ++i) {
        float distance_in_meters = distanceArr[i] / 0.0254;
        float Z = distance_in_meters / std::cbrtf(W_Kg);
        float kPa = (1772 / std::pow(Z, 3.0f)) - (114 / std::pow(Z, 2.0f)) + 108 / Z;
        float PSI = kPa / 6.89475729;
        arr[i] = PSI;
    }
}

void calculateImpulseValue(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr, int size) {
    
    auto computedValue = [](const float &distance)->float {
        float x = log10f(distance);  // compute once
        float x2 = x * x;
        float x3 = x2 * x;
        float x4 = x2 * x2;

        if (distance < 0.132f)
        {
            //return std::numeric_limits<float>::quiet_NaN();
            return 0.0f;
        }
        else if (distance < 1.69f)
        {
            return powf(10.0f,
                2.9274f
                - 1.6466f * x
                - 0.90652f * x2
                - 0.35118f * x3
                - 0.094865f * x4
            );
        }
        else if (distance < 25.21f)
        {
            return powf(10.0f,
                2.8735f
                - 1.2134f * x
                - 2.2395f * x2
                + 1.8364f * x3
                - 0.42023f * x4
            );
        }
        else if (distance <= 100.0f)
        {
            return powf(10.0f,
                1.1642f
                + 0.13928f * x
                - 0.7688f * x2
                + 1.2882f * x3
            );
        }
        else
        {
            //return std::numeric_limits<float>::quiet_NaN();
            return 0.0f;
        }
    };

    #pragma omp parallel for default(none)
    for (int i = 0; i < size; ++i) {
        float computed_value = computedValue(distanceArr[i]);
        float PSI = computed_value / 6.89475729;
        arr[i] = PSI;
    }
}

void calculatePeakPressuePerTile(const std::unique_ptr<float[]>& distanceArr, const std::unique_ptr<float[]>& basicPressureArr,
    const std::unique_ptr<float[]>& impulsePressureArr, std::unique_ptr<float[]>& arr, int size) {

    #pragma omp parallel for default(none)
    for (int i = 0; i < size; ++i) {
        float s = Detonation_Height_Inches / distanceArr[i];
        float temp = impulsePressureArr[i] * (1 + s) - 2 * pow(s, 2) + basicPressureArr[i] * pow(s, 2); //TODO rename these
        float PSI = temp / 6.89475729;
        arr[i] = PSI;
    }
}

void calculateTimeOfArrivalPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr, int size) {
    const float Charge_Weight_Pounds = 1000.0f; 
    
    auto result = [](float x)->float {
        float x2 = x * x;
        float x3 = x2 * x;

        if (x < 0.3f)
        {
            //result = std::numeric_limits<float>::quiet_NaN();
            return 0.0f;
        }
        else if (x < 2.4f)
        {
            return 0.01769362f
                - 0.02032568f * x
                + 0.5395856f * x2
                - 0.03010011f * x3;
        }
        else if (x < 12.0f)
        {
            return -2.251241f
                + 1.76582f * x
                + 0.1140477f * x2
                - 0.004066734f * x3;
        }
        else if (x <= 500.0f)
        {
             return -6.852501f
                + 2.907447f * x
                + 0.00009466282f * x2
                - 0.00000009344539f * x3;
        }
        else
        {
            //result = std::numeric_limits<float>::quiet_NaN();
            return 0.0f;
        }
    };


    #pragma omp parallel for default(none)
    for (int i = 0; i < size; ++i) {
        float x = (distanceArr[i] * 0.3048f) / std::cbrtf(Charge_Weight_Pounds);
        float value = result(x);
        arr[i] = value;
    }
}

void calculateLoadDurationPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr, int size) {
    const float Charge_Weight_Pounds = 1000.0f;
    
    #pragma omp parallel for default(none)
    for (int i = 0; i < size; ++i) {
        float x = (distanceArr[i] * 0.3048f) / std::cbrtf(Charge_Weight_Pounds);
        float numerator = 980.0f * (1.0f + powf(x / 0.54f, 10.0f));
        float denominator =
            (1.0f + powf(x / 0.02f, 3.0f)) *
            (1.0f + powf(x / 0.74f, 6.0f)) *
            sqrtf(1.0f + powf(x / 6.9f, 2.0f));

        float value = std::cbrtf(Charge_Weight_Pounds) * (numerator / denominator);
        arr[i] = value;
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
    int threads = 2
        ;
    omp_set_num_threads(threads);

    

    allocateArray(bridge_tile_distance_from_detonation_array, Array_Size);

    allocateArray(blast_to_tile_theta_array, Array_Size);

    allocateArray(blast_tile_ground_distance_to_detonation_array, Array_Size);

    allocateArray(blast_tile_basic_peak_pressure, Array_Size);

    allocateArray(impulse_value_per_tile, Array_Size);

    allocateArray(peak_pressure_per_tile, Array_Size);

    allocateArray(time_of_arrival_per_tile, Array_Size);

    allocateArray(load_duration_per_tile, Array_Size);

    //TODO: Surround all this with for-loop based on trial_amount.

    double start_time = omp_get_wtime();

    //This should be D1 from the diagram
    computeDistanceBlastDetonationToTiles(bridge_tile_distance_from_detonation_array, Array_Size);

    //This should be Z1 from the diagram
    computeGroundDistanceDetonationToTiles(blast_tile_ground_distance_to_detonation_array, Array_Size);

    //This should be A1 from the diagram
    computeAngleFromDetenationToTile(bridge_tile_distance_from_detonation_array, blast_to_tile_theta_array, Array_Size);
   
    //This should implement the third equation
    computeBasicPeakPressureForTiles(bridge_tile_distance_from_detonation_array, blast_tile_basic_peak_pressure, Array_Size);

    calculateImpulseValue(bridge_tile_distance_from_detonation_array, impulse_value_per_tile, Array_Size);

    calculatePeakPressuePerTile(bridge_tile_distance_from_detonation_array, blast_tile_basic_peak_pressure, impulse_value_per_tile, peak_pressure_per_tile, Array_Size);

    // in milliseconds
    calculateTimeOfArrivalPerTile(bridge_tile_distance_from_detonation_array, time_of_arrival_per_tile, Array_Size);

    // in milliseconds
    calculateLoadDurationPerTile(bridge_tile_distance_from_detonation_array, time_of_arrival_per_tile, Array_Size);


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
