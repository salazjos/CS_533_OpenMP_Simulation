//UNM CS 533
// Joseph Salazar
// Diego Ornelas

#pragma once

#include <memory>
#include <fstream>
#include <span>


/**
 * @class BridgeSim
 * @brief Simulates blast wave pressure propagation across a bridge surface grid.
 *
 * The simulation computes blast-related pressure values for each bridge tile
 * over time using precomputed blast physics equations. Intermediate simulation
 * values are stored in dynamically allocated arrays for efficient processing
 * and OpenMP parallelization.
 */

class BridgeSim
{

/**
		* @brief Constructs a bridge blast simulation instance.
		*
		* Initializes the bridge dimensions, detonation location, threading
		* configuration, and output file behavior.
		*
		* @param bridge_length Length of the bridge in inches.
		* @param bridge_width Width of the bridge in inches.
		* @param detonation_height Height of the detonation above the bridge surface in inches.
		* @param detonation_x X-coordinate of the detonation location.
		* @param detonation_y Y-coordinate of the detonation location.
		* @param thread_Amount Number of CPU threads to use during simulation.
		* @param produceBinaryFloatFile Determines whether simulation output is written to a binary file.
		*/
public:
	BridgeSim(int bridge_length, int bridge_width, int detonation_height, int detonation_x, 
		int detonation_y, int thread_Amount, bool produceBinaryFloatFile);

	/**
	 * @brief Executes the complete bridge blast simulation.
	 *
	 * Performs setup, precomputation, and time-stepped pressure calculations
	 * across all bridge tiles.
	 */
	void beginSimulation();

private:

	/**
	 * @brief Initializes simulation resources and allocates required arrays.
	 */
	void setup();

	/**
	 * @brief Precomputes all static blast values prior to time-stepped simulation.
	 *
	 * This includes distances, angles, pressures, arrival times,
	 * and load durations for every bridge tile.
	 */
	void preCompute();

	/**
	 * @brief Allocates a floating-point array for tile-based simulation data.
	 *
	 * @param ptr Unique pointer reference that receives the allocated array.
	 */
	void allocateArray(std::unique_ptr<float[]>& ptr);

	/**
	 * @brief Computes the slanted distance from the detonation point to each tile.
	 *
	 * @param slantDistancArray Output array containing computed slant distances.
	 */
	void computeDistanceBlastDetonationToTiles(std::unique_ptr<float[]>& slantDistancArray);

	/**
	 * @brief Computes scaled blast distances for all tiles.
	 *
	 * Scaled distance values are commonly used in blast pressure equations.
	 *
	 * @param slantDistanceArray Input array of slant distances.
	 * @param scaledDistanceArray Output array of scaled distances.
	 */
	void computeScaledDistance(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& scaledDistanceArray);

	/**
	 * @brief Computes the blast incidence angle between the detonation and each tile.
	 *
	 * @param slantDistanceArray Input array of slant distances.
	 * @param blastAngleArray Output array containing blast angles.
	 */
	void computeAngleFromDetenationToTile(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& blastAngleArray);

	/**
	 * @brief Computes the basic peak pressure value for each tile.
	 *
	 * @param scaledDistanceArray Input array of scaled distances.
	 * @param basicPressureArray Output array containing basic peak pressures.
	 */
	void computeBasicPeakPressureForTiles(
		const std::unique_ptr<float[]>& scaledDistanceArray, 
		std::unique_ptr<float[]>& basicPressureArray);

	/**
	 * @brief Calculates impulse pressure values for all tiles.
	 *
	 * @param scaledDistanceArray Input array of scaled distances.
	 * @param implusePressureArray Output array containing impulse pressures.
	 */
	void calculateImpulsePressureValue(
		const std::unique_ptr<float[]>& scaledDistanceArray, 
		std::unique_ptr<float[]>& implusePressureArray);

	/**
	 * @brief Calculates the final peak pressure for each tile.
	 *
	 * Combines impulse pressure, blast angle, and base pressure values
	 * into a resulting peak pressure value.
	 *
	 * @param impulsePressureArray Input array of impulse pressures.
	 * @param blastAngleArray Input array of blast angles.
	 * @param basicPressureArray Input array of base pressures.
	 * @param peakPressureArray Output array containing peak pressures.
	 */
	void calculatePeakPressurePerTile(
		const std::unique_ptr<float[]>& impulsePressureArray,
		const std::unique_ptr<float[]>& blastAngleArray,
		const std::unique_ptr<float[]>& basicPressureArray,
		std::unique_ptr<float[]>& peakPressureArray);

	/**
	 * @brief Calculates blast wave arrival time for each tile.
	 *
	 * @param slantDistanceArray Input array of slant distances.
	 * @param arrivalTimeArray Output array containing arrival times.
	 */
	void calculateTimeOfArrivalPerTile(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& arrivalTimeArray);

	/**
	 * @brief Calculates blast load duration for each tile.
	 *
	 * @param slantDistanceArray Input array of slant distances.
	 * @param loadDurationArray Output array containing load durations.
	 */
	void calculateLoadDurationPerTile(
		const std::unique_ptr<float[]>& slantDistanceArray, 
		std::unique_ptr<float[]>& loadDurationArray);

	/**
	 * @brief Calculates blast pressure departure times for each tile.
	 *
	 * Departure time is computed using arrival time minus load duration.
	 *
	 * @param timeArrivalArray Input array of arrival times.
	 * @param loadDurationArray Input array of load durations.
	 * @param departureTimeArray Output array containing departure times.
	 */
	void calculateTimeOfDeparturePerTile(
		const std::unique_ptr<float[]>& timeArrivalArray,
		const std::unique_ptr<float[]>& loadDurationArray,
		std::unique_ptr<float[]>& departureTimeArray);

	/**
	 * @brief Calculates active pressure values for all tiles at a given simulation time.
	 *
	 * Computes the active pressure value per tile at the current time. 
	 *
	 * @param currentTime Current simulation time in milliseconds.
	 * @param peakPressureArray Input array of peak pressures.
	 * @param arrivalTimeArray Input array of arrival times.
	 * @param loadDurationArray Input array of load durations.
	 * @param departureTimeArray Input array of departure times.
	 * @param activePressureArray Output array containing active pressures.
	 */
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
	const float Millisecond_Timestep = 1.0f;
	std::string outFname = "pressureData.bin";
};

