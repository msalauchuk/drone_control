#ifndef DRONE_H
#define DRONE_H
#include "config.h"
#include "con2redis.h"
#include <vector>
#include <math.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <hiredis/hiredis.h>

typedef struct {
    int sax; //subarea x 
    int say; //subarea y
    int ny;  // // First target x (center of first cell)
    int nx;  //
    int dx;  // direction x
    int dy; //usually 0 but changes when doing lawn mowner
} Job;
double get_random_charge_time();
void reset_job(Job * job);
Job * get_job_from_reply(redisReply * reply);


enum flying_status {
    MOVING_TO_TARGET, //  to target (center of grid point)
    COVERAGE_PATTERN, // lawn mowner pattern
    HANDOVER_SCAN,
    RETURN_TO_BASE,
    PATTERN_RESET
};


// Each drone goes through these states:
enum drone_status{
    STARTUP, // Receives job from Redis with subarea assignment
    READY, // Ready for new job
    ON_MISSION, //arrive at start pos
    HANDOVER_MODE, // flying to the last pos while other drone arrives
    HOMING, // Return to CC
    CHARGING
};
class Drone {
private:
    // Attributi privati della classe Drone
    double velx;
    double vely;
    double speed; //// 30 km/h to m/s * 2.4s = 20m per tick
    double charge_time;
    // the drone can never be at distance autonomy left 'd' away from its distance from the center 'c'.  if d>=c then the drone needs to go back to charge

public:
    int last_v_x ;
    int last_v_y;
    int last_t_v;
    double distanceToCC; // only when waiting other drone
    double x;
    double y;
    flying_status f_status;
    double battery;// 1800s = 30 minutes total flight time (100.0 / (1800.0 / T)
    drone_status status;
    int id; // Identificativo unico del drone
    Job * job;
    // Costruttore e metodi pubblici della classe Drone
    Drone(int id);
    void tick(redisContext *c,int t);
    double get_distance_control_center();
    bool is_charged();
    void charge();
    bool is_home();
    bool at_range_limit();
    double get_autonomy();
    bool is_low_battery();
    void set_last_dist();
    void send_replace_drone_msg(redisContext * c);
    void calc_velocity(double targetX, double targetY);
    void move(int t);
};
class Swarm {
    private:
        int pid;
        redisContext * c; 
        const char * sync_stream = "sync_stream2";
        const char * drone_stream = "drone_stream";
        const char * log_stream = "log_stream";

        int block_time = 10000000; //timeout redis msgs
    public:
        std::vector<Drone> drones;
        void init();
        void addDrone(Drone drone);
        void init_drones(int n); // initializes the drones
        void await_sync(); //sync time w/ redis
        void tick(int t); // time tick
        void log(); // logs
        void shutdown();// close connections
};
#endif // DRONE_H
