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

	//void beginSimulation();
	void beginSimulation();

private:

	void setup();

	void preCompute();

	void allocateArray(std::unique_ptr<float[]>& ptr);

	void computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& slantDistancArray);

	//void computeGroundDistanceDetonationToTiles(std::unique_ptr<float[]>& arr); //NOT Currently Using

	void computeScaledDistance(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& scaledDistanceArray);

	void computeAngleFromDetenationToTile(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& blastAngleArray);

	void computeBasicPeakPressureForTiles(
		const std::unique_ptr<float[]>& scaledDistanceArray, 
		std::unique_ptr<float[]>& basicPressureArray);

	void calculateImpulsePressureValue(
		const std::unique_ptr<float[]>& scaledDistanceArray, 
		std::unique_ptr<float[]>& implusePressureArray);

	void calculatePeakPressuePerTile(
		const std::unique_ptr<float[]>& impulsePressureArray,
		const std::unique_ptr<float[]>& slantedDistanceArray,
		const std::unique_ptr<float[]>& basicPressureArray,
		std::unique_ptr<float[]>& implusePressureArray);

	void calculateTimeOfArrivalPerTile(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& arrivalTimeArray);

	void calculateLoadDurationPerTile(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& loadDurationArray);

	void calculateTimeOfDeparturePerTile(
		const std::unique_ptr<float[]>& timeArrivalArray,
		const std::unique_ptr<float[]>& loadDurationArray,
		std::unique_ptr<float[]>& departureTimeArray);

	// Need to pass current time as float to the function as well- passed in by main loop
	void calculatePressurePerTileForGivenTimeValue(
		const float currentTime, 
    	const std::unique_ptr<float[]>& peakPressureArray,
    	const std::unique_ptr<float[]>& arrivalTimeArray, 
    	const std::unique_ptr<float[]>& loadDurationArray,
		const std::unique_ptr<float[]>& departureTimeArray, 
    	std::unique_ptr<float[]>& activePressureArray);

	std::unique_ptr<float[]> slanted_distance_from_detonation_array; 
	std::unique_ptr<float[]> blast_angle_array; 
	std::unique_ptr<float[]> basic_peak_pressure_array; 
	std::unique_ptr<float[]> impulse_pressure_array; 
	std::unique_ptr<float[]> peak_pressure_array;
	std::unique_ptr<float[]> arrival_time_array;
	std::unique_ptr<float[]> load_duration_array;
	std::unique_ptr<float[]> departure_time_array;
	std::unique_ptr<float[]> scaled_distance_array; 
	std::unique_ptr<float[]> active_pressure_array;

	int bridge_Length_Inches = 0; 
	int bridge_Width_Inches = 0; 
	int detonation_Height_Inches = 0; 
	int detonation_X_Location = 0; 
	int detonation_Y_Location = 0; 
	int total_Tiles = 0;

	int thread_Amount = 0;
	bool doesProduceBinaryFile = false;

	float firstTimeOfArrival = 0.0f;
	float lastTimeOfDeparture = 0.0f;
	
	const float Charge_Weight_Pounds = 1000.0f;
	const float Half_Millisecond_Timestep = 0.5f;
	std::string outFname = "pressureData.bin";
};

