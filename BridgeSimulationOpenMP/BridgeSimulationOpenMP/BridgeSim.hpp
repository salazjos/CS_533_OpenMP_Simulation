//UNM CS 533

#pragma once

#include <memory>
#include <fstream>
#include <span>

class BridgeSim
{

public:
	BridgeSim(int bridge_length, int bridge_width, int detonation_height, int detonation_x, 
		int detonation_y, int thread_Amount, bool produceBinaryFloatFile);

	void beginSimulation();

private:

	void setup();

	void preCompute();

	void allocateArray(std::unique_ptr<float[]>& ptr);

	void computeScaledDistance(const std::unique_ptr<float[]>& slantDistanceArr, std::unique_ptr<float[]>& arr);

	void computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& arr);

	void computeGroundDistanceDetonationToTiles(std::unique_ptr<float[]>& arr);

	void computeAngleFromDetenationToTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& angleArr);

	void computeBasicPeakPressureForTiles(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculateImpulsePressureValue(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculatePeakPressuePerTile(
		const std::unique_ptr<float[]>& impulsePressureArray,
		const std::unique_ptr<float[]>& slantedDistanceArray,
		const std::unique_ptr<float[]>& basicPressureArray,
		std::unique_ptr<float[]>& outArray);

	void calculateTimeOfArrivalPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculateLoadDurationPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculateTimeOfDeparturePerTile(
		const std::unique_ptr<float[]>& time_of_arrival,
		const std::unique_ptr<float[]>& load_duration,
		std::unique_ptr<float[]>& arr);

	bool continueSimulation(const std::unique_ptr<float[]>& arr, const float currentTime);

	// Need to pass current time as float to the function as well- passed in by main loop
	void calculatePressurePerTileForGivenTimeValue(
		const float currentTime, 
    	const std::unique_ptr<float[]>& peakPressureArr,
    	const std::unique_ptr<float[]>& arrivalTimeArr, 
    	const std::unique_ptr<float[]>& loadDurationArr,
		const std::unique_ptr<float[]>& departureTimeArr, 
    	std::unique_ptr<float[]>& outArr);

	void writePressurePerTileToBinaryFile(std::ofstream *file, const std::unique_ptr<float[]>& arr);

	std::unique_ptr<float[]> slanted_distance_from_detonation_array;
	std::unique_ptr<float[]> blast_to_tile_theta_array;
	std::unique_ptr<float[]> blast_tile_ground_distance_to_detonation_array;
	std::unique_ptr<float[]> blast_tile_basic_peak_pressure;
	std::unique_ptr<float[]> impulse_pressure_value_per_tile;
	std::unique_ptr<float[]> peak_pressure_per_tile;
	std::unique_ptr<float[]> time_of_arrival_per_tile;
	std::unique_ptr<float[]> load_duration_per_tile;
	std::unique_ptr<float[]> time_of_departure_per_tile;
	std::unique_ptr<float[]> active_pressure_per_tile;
	std::unique_ptr<float[]> scaled_distance;

	int bridge_Length_Inches = 0; //= 900;
	int bridge_Width_Inches = 0; //= 100;
	int detonation_Height_Inches = 0; // = 30 * 12;
	int detonation_X_Location = 0; // 800 * 12;
	int detonation_Y_Location = 0; // 50 * 12;
	int total_Tiles = 0;

	int thread_Amount = 0;
	float firstTimeOfArrival = 0.0f;
	const float Charge_Weight_Pounds = 1000.0f;

	bool doesProduceBinaryFloatFile = false;

	const float Milli_Second_Value = 0.0001f;

	std::string outFname = "pressureData.bin";
};

