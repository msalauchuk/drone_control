#ifndef CONTROL_CENTER_H
#define CONTROL_CENTER_H
#include <vector>
#include <time.h>
#include "src/common/system_config.h"
#include "../../redis/src/connector.h"
#include <math.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <hiredis/hiredis.h>


typedef struct
{
    int sax; //subarea x
    int say; //subarea y
    int ny; // next point of the subarea to start
    int nx; //
    int dx; // direction x
    int dy;
} Job;

void serialize_job_msg(const Job* my_job, char* buffer);
Job* create_job(int sax, int say, int ny, int nx, int dx, int dy);

enum DRONE_STATUS
{
    startup,
    onMission,
    homing,
    charging
};


class drone
{
public:
    explicit drone(int drone_id)
    {
        this->id = drone_id;
        last_verified_x = -1;
        last_verified_y = -1;
        battery_lvl = 100.0;
        status = startup;
    }

    int last_verified_y; // last verified pos at t
    int last_verified_x;
    double battery_lvl;
    int id;
    DRONE_STATUS status;
    Job* job;
};

enum Status
{
    STARTUP,
    READY,
    RUNNING,
    STATUS_COUNT
};

class cc
{
    int pid;
    std::vector<drone> drones; // vector of drones ids, make another drone class
    std::vector<Job*> job_queue;
    int grid[WIDTH][HEIGHT]; // griglia 2d of time stamps in unix epoch, will be used to calculate verified time
    Status status;
    bool area_verified;
    redisContext* c;
    const char* drone_ack_stream = "drone_ack";
    const char* drone_stream = "drone_stream";
    const char* log_stream = "log_stream";
    int syncTimeout = 10000000; //timeout redis msgs

public:
    cc();
    void init_redis();
    void sync_and_wait();
    void process_simulation_step();
    void initialize_grid();

    double calculate_verified_percentage(int curr_time);
    void dumpsubarea(int curr);
    void print_status() const;
    void handle_msg(const char* type, redisReply* reply);
    void log_event(int time);
    void shutdown();

    void send_substitute_drone(int new_drone);
    void addDrone(drone drone);

    int get_available_drone_id();
    bool verify_area(int t) const;



};

void save_to_csv(bool verified_area, int t, std::string filename);
void print_simulation_configuration();
bool verified_single_point(int last_v, int current_time); // checks single point
const char* int_to_string(int x);


#endif // CONTROL_CENTER_H
