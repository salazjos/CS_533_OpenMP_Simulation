//UNM CS 533
#define _CRT_SECURE_NO_WARNINGS

#include "BridgeSim.hpp"

#include <omp.h>
#include <cmath>
#include <numbers>
#include <iostream>
#include <ranges>
#include <algorithm>
#include <filesystem>

BridgeSim::BridgeSim(int bridge_length, int bridge_width,
    int detonation_height, int detonation_x, int detonation_y, int thread_Amount, bool produceBinaryFloatFile)
    : bridge_Length_Inches(bridge_length * 12),
    bridge_Width_Inches(bridge_width * 12),
    detonation_Height_Inches(detonation_height * 12),
    detonation_X_Location(detonation_x * 12),
    detonation_Y_Location(detonation_y * 12),
    thread_Amount(thread_Amount),
    doesProduceBinaryFile(produceBinaryFloatFile)
{
    setup(); 
}

void BridgeSim::beginSimulation() {

    auto printElapsedTime = [](double start_time, double end_time) {
        double elapsed_time = end_time - start_time;
        // TODO: the bash script should see this. 
        // example: output=$(./bridgeSim)
        std::cout << elapsed_time << std::endl;
        };
        
    std::ofstream outFile;
        
    if (doesProduceBinaryFile) {
        outFile.open(outFname, std::ios::out | std::ios::binary);
        int32_t tileCount = total_Tiles;
        outFile.write(reinterpret_cast<const char*>(&tileCount), sizeof(tileCount));
    }
        
    int fileWriteAmount = 0;
        
    //begin simulation----------------------------------------------------------
    bool done = false;
    float current_time = 0.0f;
    double start_time = omp_get_wtime();
    preCompute();
    #pragma omp parallel
    {
        auto* peak = peak_pressure_array.get();
        auto* toa = arrival_time_array.get();
        auto* dur = load_duration_array.get();
        auto* dep = departure_time_array.get();
        auto* out = active_pressure_array.get();

        while (true) {

            float t = 0.0f;
            #pragma omp single copyprivate(t)
            {
                if (current_time > lastTimeOfDeparture) {
                    done = true;
                }
                else {
                    t = current_time;
                    current_time += Half_Millisecond_Timestep;
                }
            }

            // broadcast barrier is implicit after 'single'

            if (done)
                break;

            #pragma omp for schedule(static)
            for (int i = 0; i < total_Tiles; i++) {
                float val;

                if (t < toa[i] || t > dep[i]) {
                    val = 0.0f;
                }
                else {
                    float ratio = (t - toa[i]) / dur[i];
                    val = peak[i] * (1.0f - ratio);
                }

                out[i] = val;
            }

            #pragma omp single
            {
                if (doesProduceBinaryFile) {
                    outFile.write(reinterpret_cast<const char*>(out),
                        total_Tiles * sizeof(float));
                    fileWriteAmount++;
                }
            }
        }
    }

    double end_time = omp_get_wtime();
    //end simulation-----------------------------------------------------------
    
    std::cout << "time simulation stopped: " << current_time << "\n";
    
    std::cout << "Elapsed time: ";
    printElapsedTime(start_time, end_time);
    std::cout << "Thread amount: " << thread_Amount << "\n";
    
    std::cout << "write file amount: " << fileWriteAmount << "\n";
    
    if (outFile.is_open())
        outFile.close();
    
    if (doesProduceBinaryFile)
        std::cout << "File Size: " << std::filesystem::file_size(outFname) / 1e9 << " GB\n";
}
  

void BridgeSim::preCompute() {
    //This should be D1 from the diagram
    computeDistanceBlastDetonationToTiles(
        slanted_distance_from_detonation_array);

    //This should be A1 from the diagram
    computeAngleFromDetenationToTile(
        slanted_distance_from_detonation_array,
        blast_angle_array);

    computeScaledDistance(
        slanted_distance_from_detonation_array,
        scaled_distance_array);

    calculateImpulsePressureValue(
        scaled_distance_array,
        impulse_pressure_array);

    //This should implement the third equation
    computeBasicPeakPressureForTiles(
        scaled_distance_array,
        basic_peak_pressure_array);

    calculatePeakPressuePerTile(
        impulse_pressure_array,
        slanted_distance_from_detonation_array,
        basic_peak_pressure_array,
        peak_pressure_array);
       
    // in milliseconds
    calculateTimeOfArrivalPerTile(
        slanted_distance_from_detonation_array,
        arrival_time_array);

    // in milliseconds
    calculateLoadDurationPerTile(
        slanted_distance_from_detonation_array,
        load_duration_array);

    // sum of arrivel & duration times
    calculateTimeOfDeparturePerTile(
        arrival_time_array,
        load_duration_array,
        departure_time_array);

    std::span<float> arriveView(arrival_time_array.get(), total_Tiles);
    firstTimeOfArrival = *std::ranges::min_element(arriveView);
    std::cout << "first time of arrival: " << firstTimeOfArrival << "\n";

    std::span<float> departView(departure_time_array.get(), total_Tiles);
    lastTimeOfDeparture = *std::ranges::max_element(departView);
    std::cout << "last time of departure: " << lastTimeOfDeparture << "\n";

    // Clear out these arrays as their data is no longer needed.
    slanted_distance_from_detonation_array.reset();
    blast_angle_array.reset();
    basic_peak_pressure_array.reset();
    impulse_pressure_array.reset();
    scaled_distance_array.reset();
}

void BridgeSim::allocateArray(std::unique_ptr<float[]>& ptr) {
    ptr = std::make_unique<float[]>(total_Tiles);
}

void BridgeSim::setup() {
    
    // Include OMP folder in output file path if launched by script
    const char* expFlag = std::getenv("EXP");
    std::string expValue = expFlag == nullptr ? "" : std::string(expFlag);
    if (expValue.compare("Script") == 0)
        outFname = "BridgeSimulationOpenMP/" + outFname;
    
    // Set the number of threads
    omp_set_num_threads(thread_Amount);
    
    total_Tiles = bridge_Length_Inches * bridge_Width_Inches;

    // Allocate memeory for all arrays.
    try {
        allocateArray(slanted_distance_from_detonation_array);
        allocateArray(blast_angle_array);
        allocateArray(basic_peak_pressure_array);
        allocateArray(impulse_pressure_array);
        allocateArray(peak_pressure_array);
        allocateArray(arrival_time_array);
        allocateArray(load_duration_array);
        allocateArray(departure_time_array);
        allocateArray(active_pressure_array);
        allocateArray(scaled_distance_array);
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

void BridgeSim::computeScaledDistance(const std::unique_ptr<float[]>& slantDistanceArr, std::unique_ptr<float[]>& outArray) {
    float charge_weight = Charge_Weight_Pounds;
    const int total = total_Tiles;
    
    #pragma omp parallel for default(none) shared(total, charge_weight, slantDistanceArr, outArray)
    for (int i = 0; i < total; ++i) {
        float Z = slantDistanceArr[i] / std::cbrtf(Charge_Weight_Pounds);
        outArray[i] = Z;
    }
}

void BridgeSim::computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& outArray) {
    int Length = bridge_Length_Inches;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, Length, outArray)
    for (int i = 0; i < total; ++i){
        int x = i % Length;
        int y = i / Length;

        double dx = x - detonation_X_Location;
        double dy = y - detonation_Y_Location;
        double dz = detonation_Height_Inches;

        double dist_ft = std::sqrt(dx * dx + dy * dy + dz * dz) / 12.0;

        outArray[i] = static_cast<float>(dist_ft);
    }   
}

void BridgeSim::computeAngleFromDetenationToTile(const std::unique_ptr<float[]>& slantDistanceArray, std::unique_ptr<float[]>& outArray) {

    const float dz = detonation_Height_Inches;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, slantDistanceArray, outArray, dz)
    for (int i = 0; i < total; ++i) {
        float ratio = dz / slantDistanceArray[i];
        ratio = fminf(1.0f, fmaxf(-1.0f, ratio));
        float theta = acosf(ratio);
        float degrees = theta * (180.0f / std::numbers::pi_v<float>);
        outArray[i] = degrees;
    }
}

void BridgeSim::calculatePeakPressuePerTile(
    const std::unique_ptr<float[]>& impulsePressureArray, 
    const std::unique_ptr<float[]>& slantDistanceArray, 
    const std::unique_ptr<float[]>& basicPressureArray,
    std::unique_ptr<float[]>& outArray) {

    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, slantDistanceArray, basicPressureArray, impulsePressureArray, outArray)
    for (int i = 0; i < total; ++i) {
        float s = detonation_Height_Inches / slantDistanceArray[i];
        float value = impulsePressureArray[i] * ((1 + s) - 2 * pow(s, 2)) + basicPressureArray[i] * pow(s, 2);
        //float PSI = temp / 6.89475729;
        outArray[i] = value;
    }
}

void BridgeSim::calculateLoadDurationPerTile(const std::unique_ptr<float[]>& slantDistanceArray, std::unique_ptr<float[]>& outArray) {
    const float Charge_Weight_Pounds = 1000.0f;
    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, Charge_Weight_Pounds, slantDistanceArray, outArray)
    for (int i = 0; i < total; ++i) {
        float x = (slantDistanceArray[i] * 0.3048f) / std::cbrtf(Charge_Weight_Pounds);
        float numerator = 980.0f * (1.0f + powf(x / 0.54f, 10.0f));
        float denominator =
            (1.0f + powf(x / 0.02f, 3.0f)) *
            (1.0f + powf(x / 0.74f, 6.0f)) *
            sqrtf(1.0f + powf(x / 6.9f, 2.0f));

        float value = std::cbrtf(Charge_Weight_Pounds) * (numerator / denominator);
        outArray[i] = value;
    }
}

void BridgeSim::calculateTimeOfDeparturePerTile(const std::unique_ptr<float[]>& timeArrivalArray,
    const std::unique_ptr<float[]>& loadDurationArray,
    std::unique_ptr<float[]>& outArray) {

    const int total = total_Tiles;

    #pragma omp parallel for default(none) shared(total, timeArrivalArray, loadDurationArray, outArray)
    for (int i = 0; i < total; ++i) {
        auto time_of_departure = timeArrivalArray[i] + loadDurationArray[i];
        outArray[i] = time_of_departure;
    }
}

// Implemented P(t) function
void BridgeSim::calculatePressurePerTileForGivenTimeValue(
    const float currentTime,
    const std::unique_ptr<float[]>& peakPressureArray,
    const std::unique_ptr<float[]>& timeOfArrivalArray,
    const std::unique_ptr<float[]>& loadDurationArray,
    const std::unique_ptr<float[]>& departureOfTimeArray,
    std::unique_ptr<float[]>& outArray) {

    auto* peak = peakPressureArray.get();
    auto* toa = timeOfArrivalArray.get();
    auto* dur = loadDurationArray.get();
    auto* dep = departureOfTimeArray.get();
    auto* out = outArray.get();

    //int activeCount = 0;

    #pragma omp parallel for schedule(static) //reduction(+:activeCount)
    for (int i = 0; i < total_Tiles; i++) {
        float val;

        if (currentTime < toa[i] || currentTime > dep[i]) {
            val = 0.0f;
        }
        else {
            float ratio = (currentTime - toa[i]) / dur[i];
            val = peak[i] * (1.0f - ratio);
        }

        out[i] = val;
        //if (val > 0.0f)
        //    activeCount++;
    }

    //return (activeCount > 0) || (currentTime <= firstTimeOfArrival + timestepInterval);
   /* #pragma omp parallel for schedule(static)
    for (int i = 0; i < total_Tiles; i++) {
        if (currentTime < toa[i] || currentTime > dep[i]) {
            out[i] = 0.0f;
        }
        else {
            float ratio = (currentTime - toa[i]) / dur[i];
            out[i] = peak[i] * (1.0f - ratio);
        }
    }*/

    /*const int arrSize = total_Tiles;

    #pragma omp parallel for schedule(static) shared(arrSize, currentTime, peakPressureArray, timeOfArrivalArray, loadDurationArray, departureOfTimeArrArray, outArray)
    for (int i = 0; i < arrSize; i++) {
        if (currentTime < timeOfArrivalArray[i] || currentTime > departureOfTimeArrArray[i]) {
            outArray[i] = 0.0f;
        }
        else {
            float timeElapsedRatio = (currentTime - timeOfArrivalArray[i]) / loadDurationArray[i];
            float currentPSI = peakPressureArray[i] * (1.0f - timeElapsedRatio);
            outArray[i] = currentPSI;
        }
    }*/
}

void BridgeSim::calculateImpulsePressureValue(const std::unique_ptr<float[]>& scaledDistanceArray, std::unique_ptr<float[]>& outArray) {

    const int total = total_Tiles;

    auto computedValue = [](const float& distance)->float {
        float x = log10f(distance);  // compute once
        float x2 = x * x;
        float x3 = x2 * x;
        float x4 = x2 * x2;

        if (distance < 0.132f)
        {
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
            return 0.0f;
        }
    };

    #pragma omp parallel for default(none) shared(total, scaledDistanceArray, outArray, computedValue)
    for (int i = 0; i < total; ++i) {
        float computed_value = computedValue(scaledDistanceArray[i]);
        float PSI = computed_value / 6.89475729;
        outArray[i] = PSI;
    }
}

void BridgeSim::computeBasicPeakPressureForTiles(const std::unique_ptr<float[]>& scaledDistanceArray, std::unique_ptr<float[]>& outArray) {

    const float Kg_Conversion_Rate = 2.205;
    const float Charge_Weight_Pounds = 1000.0f;
    const float W_Kg = Charge_Weight_Pounds / Kg_Conversion_Rate;
    const int total = total_Tiles;

    auto computedValue = [](const float& distance)->float {
        if (distance < 0.132f) {
            return 0.0f;
        }
        else if (distance < 2.64) {
            double x = std::log10(distance);
            return std::pow(10.0,
                3.8543 - 2.0054 * x - 1.075 * x * x - 0.25645 * x * x * x
            );
        }
        else if (distance < 25.21) {
            double x = std::log10(distance);
            double x2 = x * x;
            double x3 = x2 * x;
            double x4 = x3 * x;
            return std::pow(10.0,
                3.5885 - 0.193 * x - 5.7824 * x2 + 4.8069 * x3 - 1.2088 * x4
            );
        }
        else if (distance <= 100.0) {
            double x = std::log10(distance);
            return std::pow(10.0, 2.4663 - 1.3806 * x);
        }
        else {
            return 0.0f;
        }
    };

    #pragma omp parallel for default(none) shared(total, scaledDistanceArray, outArray, computedValue)
    for (int i = 0; i < total; ++i) {
        float computed_value = computedValue(scaledDistanceArray[i]);
        float PSI = computed_value / 6.89475729;
        outArray[i] = PSI;
    }

    //#pragma omp parallel for default(none) shared(total, distanceArr, outArray, Kg_Conversion_Rate, Charge_Weight_Pounds, W_Kg)
    //for (int i = 0; i < total; ++i) {
    //    //float distance_in_meters = distanceArr[i] / 0.0254;
    //    float Z = distanceArr[i] / std::cbrtf(Charge_Weight_Pounds);
    //    //float Z = distance_in_meters / std::cbrtf(W_Kg);
    //    float kPa = (1772 / std::pow(Z, 3.0f)) - (114 / std::pow(Z, 2.0f)) + 108 / Z;
    //    float PSI = kPa / 6.89475729;
    //    outArray[i] = PSI;
    //}
}

void BridgeSim::calculateTimeOfArrivalPerTile(const std::unique_ptr<float[]>& slantDistanceArray, std::unique_ptr<float[]>& outArray) {
    const float Charge_Weight_Pounds = 1000.0f;
    const int total = total_Tiles;

    auto result = [](float x)->float { 
        float x2 = x * x;
        float x3 = x2 * x;

        if (x < 0.3f)
        {
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
            return 0.0f;
        }
    };

    #pragma omp parallel for default(none) shared(total, Charge_Weight_Pounds, slantDistanceArray, outArray, result)
    for (int i = 0; i < total; ++i) {
        float x = (slantDistanceArray[i] * 0.3048f) / std::cbrtf(Charge_Weight_Pounds);
        float value = result(x);
        outArray[i] = value * std::cbrtf(Charge_Weight_Pounds);
    }
}

//void BridgeSim::computeGroundDistanceDetonationToTiles(std::unique_ptr<float[]>& outArray) {
//    int Width = bridge_Width_Inches;
//    const int total = total_Tiles;
//
//    #pragma omp parallel for default(none) shared(total, Width, outArray)
//    for (int i = 0; i < total; ++i){
//        int x = i % Width;
//        int y = i / Width;
//
//        float dx = x - detonation_X_Location;
//        float dy = y - detonation_Y_Location;
//
//        outArray[i] = sqrtf(dx * dx + dy * dy) / 12.0f;
//    }
//}

//void BridgeSim::beginSimulation() {
//
//    auto printElapsedTime = [](double start_time, double end_time) {
//        double elapsed_time = end_time - start_time;
//        // TODO: the bash script should see this. 
//        // example: output=$(./bridgeSim)
//        std::cout << elapsed_time << std::endl;
//        };
//
//    std::ofstream outFile;
//
//    if (doesProduceBinaryFile) {
//        outFile.open(outFname, std::ios::out | std::ios::binary);
//        int32_t tileCount = total_Tiles;
//        outFile.write(reinterpret_cast<const char*>(&tileCount), sizeof(tileCount));
//    }
//
//    int fileWriteAmount = 0;
//
//    //begin simulation----------------------------------------------------------
//    float current_time = 0.0f;
//    double start_time = omp_get_wtime();
//
//    preCompute();
//    while (current_time <= lastTimeOfDeparture) {
//
//        calculatePressurePerTileForGivenTimeValue(
//            current_time,
//            peak_pressure_array,
//            arrival_time_array,
//            load_duration_array,
//            departure_time_array,
//            active_pressure_array
//        );
//
//        current_time += Half_Millisecond_Timestep;
//
//        if (doesProduceBinaryFile) {
//            outFile.write(reinterpret_cast<const char*>(active_pressure_array.get()),
//                total_Tiles * sizeof(float));
//            fileWriteAmount++;
//        }
//    }
//
//    double end_time = omp_get_wtime();
//    //end simulation-----------------------------------------------------------
//
//    std::cout << "time simulation stopped: " << current_time << "\n";
//
//    std::cout << "Elapsed time: ";
//    printElapsedTime(start_time, end_time);
//    std::cout << "Thread amount: " << thread_Amount << "\n";
//
//    std::cout << "write file amount: " << fileWriteAmount << "\n";
//
//    if (outFile.is_open())
//        outFile.close();
//
//    if (doesProduceBinaryFile)
//        std::cout << "File Size: " << std::filesystem::file_size(outFname) / 1e9 << " GB\n";
//}