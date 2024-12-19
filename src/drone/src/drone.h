#ifndef DRONE_H
#define DRONE_H
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <new>
#include <vector>
#include <cmath>
#include <unistd.h>
#include "src/common/system_config.h"
#include "src/redis/src/connector.h"



enum FLIGHT_STATUS {
    MOVING_TO_TARGET, //  to target (center of grid point)
    COVERAGE_PATTERN, // lawn mowner pattern
    HANDOVER_SCAN,
    RETURN_TO_BASE,
    PATTERN_RESET
};


// Each drone goes through these states:
enum DRONE_STATUS{
    STARTUP, // Receives job from Redis with subarea assignment
    READY, // Ready for new job
    ON_MISSION, //arrive at start pos
    HANDOVER_MODE, // flying to the last pos while other drone arrives
    HOMING, // Return to CC
    CHARGING
};


typedef struct {
    int sax; //subarea x
    int say; //subarea y
    int ny;  // // First target x (center of first cell)
    int nx;  //
    int dx;  // direction x
    int dy; //usually 0 but changes when doing lawn mowner
} Job;
double calculate_random_charge_time();
void reset_patrol_pattern(Job * job);
Job * parse_job_from_reply(redisReply * reply);

class drone {
    double velx;
    double vely;
    double speed; //// 30 km/h to m/s * 2.4s = 20m per update_position
    double charge_time;

public:
    int last_v_x ;
    int last_v_y;
    int last_t_v;
    double distanceToCC; // only when waiting other drone
    double x;
    double y;
    FLIGHT_STATUS f_status;
    double battery;// 1800s = 30 minutes total flight time (100.0 / (1800.0 / T)
    DRONE_STATUS status;
    int id;
    Job * job;

    drone(int id);
    void tick(redisContext *c,int t);
    double calculate_distance_to_base();
    bool is_fully_charged();
    void charge();
    bool is_at_base();
    bool has_reached_range_limit();
    double get_autonomy();
    bool is_low_battery();
    void set_last_dist();
    void request_drone_replacement(redisContext * c);
    void calc_velocity(double targetX, double targetY);
    void update_position(int t);
};
class DroneFleet {
        int pid;
        redisContext * c; 
        const char * sync_stream = "time_sync";
        const char * drone_stream = "drone_stream";
        const char * log_stream = "log_stream";

        int block_time = 10000000; //timeout redis msgs
    public:
        std::vector<drone> drones;
        void init();
        void addDrone(drone drone);
        void init_drones(int n); // initializes the drones
        void sync_with_control_center(); //sync time w/ redis
        void update_position(int t); // time update_position
        void log(); // logs
        void shutdown();// close connections
};
#endif // DRONE_H
