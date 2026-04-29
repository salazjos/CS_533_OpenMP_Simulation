//UNM CS 533

#include "BridgeSim.hpp"

#include <omp.h>
#include <cmath>
#include <numbers>
#include <iostream>

BridgeSim::BridgeSim(int bridge_length, int bridge_width,
    int detonation_height, int detonation_x, int detonation_y, int thread_Amount, bool produceBinaryFloatFile)
    : bridge_Length_Inches(bridge_length * 12),
    bridge_Width_Inches(bridge_width * 12),
    detonation_Height_Inches(detonation_height * 12),
    detonation_X_Location(detonation_x * 12),
    detonation_Y_Location(detonation_y * 12),
    thread_Amount(thread_Amount),
    doesProduceBinaryFloatFile(produceBinaryFloatFile)
{
    total_Tiles = bridge_Length_Inches * bridge_Width_Inches;
    
    setup(); 
}

void BridgeSim::beginSimulation() {

    std::ofstream outFile("pressureData.bin", std::ios::out | std::ios::binary);
    if (outFile.is_open()) {
        //First line in the file is the array size.
        outFile.write(reinterpret_cast<const char*>(&total_Tiles), sizeof(int));
    }

    double start_time = omp_get_wtime();

    preCompute();

    //do-while loop



    
    
    writePressurePerTileToBinaryFile(&outFile, active_pressue_per_tile);

    if (outFile.is_open())
        outFile.close();

    double end_time = omp_get_wtime();
    double elapsed_time = end_time - start_time;
    // TODO: the bash script should see this. 
    // example: output=$(./bridgeSim)
    std::cout << elapsed_time << std::endl;
}

void BridgeSim::preCompute() {
    //This should be D1 from the diagram
    computeDistanceBlastDetonationToTiles(bridge_tile_distance_from_detonation_array);

    //This should be Z1 from the diagram
    computeGroundDistanceDetonationToTiles(blast_tile_ground_distance_to_detonation_array);

    //This should be A1 from the diagram
    computeAngleFromDetenationToTile(
        bridge_tile_distance_from_detonation_array, 
        blast_to_tile_theta_array);

    //This should implement the third equation
    computeBasicPeakPressureForTiles(
        bridge_tile_distance_from_detonation_array, 
        blast_tile_basic_peak_pressure);

    calculateImpulseValue(
        bridge_tile_distance_from_detonation_array, 
        impulse_value_per_tile);

    calculatePeakPressuePerTile(
        bridge_tile_distance_from_detonation_array, 
        blast_tile_basic_peak_pressure, 
        impulse_value_per_tile, peak_pressure_per_tile);

    // in milliseconds
    calculateTimeOfArrivalPerTile(
        bridge_tile_distance_from_detonation_array, 
        time_of_arrival_per_tile);

    // in milliseconds
    calculateLoadDurationPerTile(
        bridge_tile_distance_from_detonation_array, 
        time_of_arrival_per_tile);
}

void BridgeSim::allocateArray(std::unique_ptr<float[]>& ptr) {
    ptr = std::make_unique<float[]>(total_Tiles);
}

void BridgeSim::setup() {
    
    omp_set_num_threads(thread_Amount);
    try {
        allocateArray(bridge_tile_distance_from_detonation_array);
        allocateArray(blast_to_tile_theta_array);
        allocateArray(blast_tile_ground_distance_to_detonation_array);
        allocateArray(blast_tile_basic_peak_pressure);
        allocateArray(impulse_value_per_tile);
        allocateArray(peak_pressure_per_tile);
        allocateArray(time_of_arrival_per_tile);
        allocateArray(load_duration_per_tile);
        allocateArray(time_of_departure_per_tile);
        allocateArray(active_pressue_per_tile);
    }
    catch (const std::bad_alloc& e) {
        const size_t bytes_per_array = total_Tiles * sizeof(float);
        const size_t total_bytes = bytes_per_array * 8;

        std::cerr << "Fatal: memory allocation failed\n"
            << "Tiles: " << total_Tiles << '\n'
            << "Threads: " << thread_Amount << '\n'
            << "Approx bytes per array: " << bytes_per_array << '\n'
            << "Approx total bytes: " << total_bytes << '\n'
            << e.what() << '\n';

        std::exit(EXIT_FAILURE);
    }
}

void BridgeSim::computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& outArray) {
    int Width = bridge_Width_Inches;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, Width, outArray)
    for (int i = 0; i < total; ++i){
        int x = i % Width;
        int y = i / Width;

        double dx = x - detonation_X_Location;
        double dy = y - detonation_Y_Location;
        double dz = detonation_Height_Inches;

        outArray[i] = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
    }
}

void BridgeSim::computeGroundDistanceDetonationToTiles(std::unique_ptr<float[]>& outArray) {
    int Width = bridge_Width_Inches;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, Width, outArray)
    for (int i = 0; i < total; ++i){
        int x = i % Width;
        int y = i / Width;

        float dx = x - detonation_X_Location;
        float dy = y - detonation_Y_Location;

        outArray[i] = sqrtf(dx * dx + dy * dy);
    }
}

void BridgeSim::computeAngleFromDetenationToTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& outArray) {

    const float dz = detonation_Height_Inches;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, distanceArr, outArray, dz)
    for (int i = 0; i < total; ++i) {
        float ratio = dz / distanceArr[i];
        ratio = fminf(1.0f, fmaxf(-1.0f, ratio));
        float theta = acosf(ratio);
        float degrees = theta * 180.0f / std::numbers::pi;
        outArray[i] = degrees;
    }
}

void BridgeSim::computeBasicPeakPressureForTiles(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& outArray) {

    const float Kg_Conversion_Rate = 2.205;
    const float Charge_Weight_Pounds = 1000.0f;
    const float W_Kg = Charge_Weight_Pounds / Kg_Conversion_Rate;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, distanceArr, outArray, Kg_Conversion_Rate, Charge_Weight_Pounds, W_Kg)
    for (int i = 0; i < total; ++i) {
        float distance_in_meters = distanceArr[i] / 0.0254;
        float Z = distance_in_meters / std::cbrtf(W_Kg);
        float kPa = (1772 / std::pow(Z, 3.0f)) - (114 / std::pow(Z, 2.0f)) + 108 / Z;
        float PSI = kPa / 6.89475729;
        outArray[i] = PSI;
    }
}

void BridgeSim::calculatePeakPressuePerTile(const std::unique_ptr<float[]>& distanceArr, const std::unique_ptr<float[]>& basicPressureArr,
    const std::unique_ptr<float[]>& impulsePressureArr, std::unique_ptr<float[]>& outArray) {

    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, distanceArr, basicPressureArr, impulsePressureArr, outArray)
    for (int i = 0; i < total; ++i) {
        float s = detonation_Height_Inches / distanceArr[i];
        float temp = impulsePressureArr[i] * (1 + s) - 2 * pow(s, 2) + basicPressureArr[i] * pow(s, 2); //TODO rename these
        float PSI = temp / 6.89475729;
        outArray[i] = PSI;
    }
}

void BridgeSim::calculateLoadDurationPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& outArray) {
    const float Charge_Weight_Pounds = 1000.0f;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, Charge_Weight_Pounds, distanceArr, outArray)
    for (int i = 0; i < total; ++i) {
        float x = (distanceArr[i] * 0.3048f) / std::cbrtf(Charge_Weight_Pounds);
        float numerator = 980.0f * (1.0f + powf(x / 0.54f, 10.0f));
        float denominator =
            (1.0f + powf(x / 0.02f, 3.0f)) *
            (1.0f + powf(x / 0.74f, 6.0f)) *
            sqrtf(1.0f + powf(x / 6.9f, 2.0f));

        float value = std::cbrtf(Charge_Weight_Pounds) * (numerator / denominator);
        outArray[i] = value;
    }
}

bool BridgeSim::isSimulationInProgress(const std::unique_ptr<float[]>& arr) {
    int numTiles = 0;
    const int total = total_Tiles;

    #pragma omp parallel for schedule(dynamic) shared(total, arr) reduction(+:numTiles)
    for (int i = 0; i < total; ++i) {
        if (arr[i] > 0.0f)
            ++numTiles;
    }

    return numTiles > 0;
}

void BridgeSim::calculateTimeOfDeparturePerTile(const std::unique_ptr<float[]>& time_of_arrival,
    const std::unique_ptr<float[]>& load_duration,
    std::unique_ptr<float[]>& outArray) {

    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, time_of_arrival, load_duration, outArray)
    for (int i = 0; i < total; ++i) {
        auto time_of_departure = time_of_arrival[i] + load_duration[i];
        outArray[i] = time_of_departure;
    }
}

void BridgeSim::calculateImpulseValue(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& outArray) {

    const int total = total_Tiles;

    auto computedValue = [](const float& distance)->float {
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

    #pragma omp parallel for default(none) shared(total, distanceArr, outArray, computedValue)
    for (int i = 0; i < total; ++i) {
        float computed_value = computedValue(distanceArr[i]);
        float PSI = computed_value / 6.89475729;
        outArray[i] = PSI;
    }
}

void BridgeSim::calculateTimeOfArrivalPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& outArray) {
    const float Charge_Weight_Pounds = 1000.0f;
    const int total = total_Tiles;

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


    #pragma omp parallel for default(none) shared(total, Charge_Weight_Pounds, distanceArr, outArray, result)
    for (int i = 0; i < total; ++i) {
        float x = (distanceArr[i] * 0.3048f) / std::cbrtf(Charge_Weight_Pounds);
        float value = result(x);
        outArray[i] = value;
    }
}


void BridgeSim::writePressurePerTileToBinaryFile(std::ofstream *filePtr, const std::unique_ptr<float[]>& arr){
    if (filePtr && filePtr->is_open()) {
        filePtr->write(reinterpret_cast<const char*>(arr.get()), total_Tiles * sizeof(float));
    }
}

//TODO Deigo
//void BridgeSim::calculatePressurePerTileForGivenTimeValue() {
//
//   // fill in 
//}