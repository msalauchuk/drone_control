#ifndef CONFIG_H
#define CONFIG_H


// Total simulation duration in seconds (approximately 9.87 hours)
#define HORIZON 35540

// Total number of drones in the swarm
#define DRONES_COUNT 1000

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

// Maximum flight distance in meters that a drone can travel on full battery
#define MAX_DISTANCE 15000.0

// Safety margin in meters for battery calculations
// Ensures drone can return to control center with buffer
#define MARGIN 500

// Debug mode flag (1 = enabled, 0 = disabled)
#define DEBUG 1

// Control Center Y coordinate (center of the 6000m x 6000m area)
#define CC_Y 3000

// Control Center X coordinate (center of the 6000m x 6000m area)
#define CC_X 3000
#endif

