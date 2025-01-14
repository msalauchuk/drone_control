#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

// Simulation Parameters
//ci sono diversi parametri che puoi modificare per testare diversi
//scenari del sistema:



// Test di Performance
#define T 2.4               // Time step in seconds
#define DEBUG 1             // Debug mode flag

//per Test di Scala:
#define TOTAL_SIMULATION_TIME 28800 // Total simulation duration in seconds (8 hours)
#define DRONES_COUNT 1000 // Total number of drones in the swarm


// Size of each subarea side in grid units (10 * 20 meters = 200m per side)
#define SUB_AREAS_SIDE 10

// Width of the total area in subareas (30 subareas * 200m = 6000m width)
#define SUB_AREAS_W 30
// Height of the total area in subareas (30 subareas * 200m = 6000m height)
#define SUB_AREAS_H 30

#define GRID_SIZE 20

// Time step(tick) in seconds (chosen to make drones move 20m per tick at 30km/h)
// Also synchronizes with 30-minute battery life (750 ticks * 2.4s = 1800s = 30min)
#define T 2.4

//Test Area di Copertura
// Grid and Area Parameters
#define WIDTH 300           // Grid width (20m spacing)
#define HEIGHT 300          // Grid height
#define GRID_SIZE 20        // Size of each grid cell in meters
#define SUB_AREAS_SIDE 10   // Size of each subarea side in grid units
#define SUB_AREAS_W 30      // Width of total area in subareas
#define SUB_AREAS_H 30      // Height of total area in subareas

// Test Limiti Operativi
// Drone Parameters
#define MAX_DISTANCE 15000.0 // Maximum flight distance on full battery (meters)
// Safety margin in meters for battery calculations
// Ensures drone can return to control center with buffer
#define MARGIN 500
#define MIN_BATTERY 1.0     // Minimum battery level


#define MAX_VERIFY_TIME 900 // Maximum verification time (15min)




// Control Center Coordinates

// Control Center Y coordinate (center of the 6000m x 6000m area)
#define CC_Y 3000

// Control Center X coordinate (center of the 6000m x 6000m area)
#define CC_X 3000



// Test Configuration
#define TEST_PATH "../test/system.csv"


#define DEBUG_REDIS 1  // Set to 1 to show Redis messages, 0 to hide them

// Simple macro for Redis debug messages
#define REDIS_LOG(format, ...) \
if (DEBUG_REDIS) { \
printf("[REDIS] " format "\n", ##__VA_ARGS__); \
}


#endif



