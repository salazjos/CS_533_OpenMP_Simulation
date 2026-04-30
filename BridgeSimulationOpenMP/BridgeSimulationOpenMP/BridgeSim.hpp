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

	void computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& arr);

	void computeGroundDistanceDetonationToTiles(std::unique_ptr<float[]>& arr);

	void computeAngleFromDetenationToTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& angleArr);

	void computeBasicPeakPressureForTiles(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculateImpulseValue(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculatePeakPressuePerTile(
		const std::unique_ptr<float[]>& distanceArr, 
		const std::unique_ptr<float[]>& basicPressureArr,
		const std::unique_ptr<float[]>& impulsePressureArr, 
		std::unique_ptr<float[]>& arr);

	void calculateTimeOfArrivalPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculateLoadDurationPerTile(const std::unique_ptr<float[]>& distanceArr, std::unique_ptr<float[]>& arr);

	void calculateTimeOfDeparturePerTile(
		const std::unique_ptr<float[]>& time_of_arrival,
		const std::unique_ptr<float[]>& load_duration,
		std::unique_ptr<float[]>& arr);

	bool isSimulationInProgress(const std::unique_ptr<float[]>& arr);

	//TODO Deigo
	// I think this is what you need. fill in the last array. I'll write that array to the binary file for every time step.
	void calculatePressurePerTileForGivenTimeValue(
		const std::unique_ptr<float[]> &peak_pressure_per_tile,
		const std::unique_ptr<float[]> &time_of_arrival_per_tile,
		const std::unique_ptr<float[]> &load_duration_per_tile,
		const std::unique_ptr<float[]> &time_of_departure_per_tile,
		std::unique_ptr<float[]> &active_pressue_per_tile);

	void writePressurePerTileToBinaryFile(std::ofstream *file, const std::unique_ptr<float[]>& arr);

	std::unique_ptr<float[]> bridge_tile_distance_from_detonation_array;
	std::unique_ptr<float[]> blast_to_tile_theta_array;
	std::unique_ptr<float[]> blast_tile_ground_distance_to_detonation_array;
	std::unique_ptr<float[]> blast_tile_basic_peak_pressure;
	std::unique_ptr<float[]> impulse_value_per_tile;
	std::unique_ptr<float[]> peak_pressure_per_tile;
	std::unique_ptr<float[]> time_of_arrival_per_tile;
	std::unique_ptr<float[]> load_duration_per_tile;
	std::unique_ptr<float[]> time_of_departure_per_tile;
	std::unique_ptr<float[]> active_pressue_per_tile;

	int bridge_Length_Inches = 0; //= 900;
	int bridge_Width_Inches = 0; //= 100;
	int detonation_Height_Inches = 0; // = 30 * 12;
	int detonation_X_Location = 0; // 800 * 12;
	int detonation_Y_Location = 0; // 50 * 12;
	int total_Tiles = 0;

	int thread_Amount = 0;
	float firstTimeOfArrival = 0.0f;
	bool doesProduceBinaryFloatFile = false;

};

