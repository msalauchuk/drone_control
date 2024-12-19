#include "drone.h"
#include "src/redis/src/connector.h"
#include <hiredis/hiredis.h>



drone::drone(int did){
    id = did;
    battery = 100,
    x = CC_X;
    y = CC_Y;
    f_status = MOVING_TO_TARGET;
    speed = (30.0 / 3.6) * T;
    status = STARTUP;
    last_v_x = 0;
    last_v_y = 0;
    last_t_v = -1;
}

void DroneFleet::addDrone(drone drone)
{
    // Aggiunge un drone al centro di controllo
    drones.push_back(drone);
}

void DroneFleet::init_drones(int n)
{
    for (int i = 0; i < n; i++)
    {
        addDrone(drone(i));
        printf("init id :%d\n", drones[i].id);
    }
}

void DroneFleet::sync_with_control_center()
{
    redisReply* reply;


    // receive synchronization messages from the Redis time_sync
    reply = stream_read_message_blocking(c, "diameter", "drone", block_time, sync_stream);
    freeReplyObject(reply);

    // we could send the time t in the stream or get it calculated from the drones
    // Send a synchronization message to the Redis drone_ack
    const char* message = "sync";
    reply = RedisCommand(c, "XADD %s * type %s", "drone_ack", message);

    if (DEBUG)
    {
        //dumpReply(reply,0);
        validate_reply_type(c, reply, REDIS_REPLY_STRING);
    }
    freeReplyObject(reply);
}


// Initialize 1000 drones
// Each drone starts at CC (3000,3000) with 100% battery
void DroneFleet::init()
{
    //set up redis context
    this->pid = getpid();
    srand(time(NULL));

    this->c = redisConnect("127.0.0.1", 6379);

    if (c == NULL || c->err)
    {
        if (c)
        {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        }
        else
        {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }
    if (DEBUG)
    {
        printf("initialized redis con\n");
        redisReply* reply = (redisReply*)redisCommand(c, "PING");
        if (reply == NULL)
        {
            printf("Error: Failed to execute PING command\n");
            redisFree(c);
            exit(1);
        }

        // Check the reply
        if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "PONG") == 0)
        {
            printf("Redis is alive!\n");
        }
        else
        {
            printf("Error: Redis did not respond properly\n");
        }

        freeReplyObject(reply);
    }
}

Job* parse_job_from_reply(redisReply* reply)
{
    //low_battery msgtype e.g. type low_battery did 123 cy 0 cx 0 nexty 40 nextx 65 dirx -1 diry -1
    char say[8],sax[8],ny[8],nx[8],dx[8], dy[8];

    get_stream_message_field_value(reply, 0, 0, 5, say);
    get_stream_message_field_value(reply, 0, 0, 7, sax);
    get_stream_message_field_value(reply, 0, 0, 9, ny);
    get_stream_message_field_value(reply, 0, 0, 11, nx);
    get_stream_message_field_value(reply, 0, 0, 13, dx);
    get_stream_message_field_value(reply, 0, 0, 15, dy);

    // Allocate and initialize new Job
    Job* job = (Job*)malloc(sizeof(Job));

    // Assign to job struct
    job->sax = std::stoi(sax);
    job->say = std::stoi(say);
    job->ny = std::stoi(ny);
    job->nx = std::stoi(nx);
    job->dx = std::stoi(dx);
    job->dy = std::stoi(dy);
    return job;
}


// charging time between 2-3 hours
double calculate_random_charge_time()
{
    // Generate a random number between min and max
    double min = 2 * 60 * 60; //2h
    double max = 3 * 60 * 60; //3h
    double randomNumber = (double)rand() / RAND_MAX * (max - min) + min;

    return randomNumber;
}

// Calculate Euclidean distance from drone to control center sqrt(dx² + dy²)
double drone::calculate_distance_to_base()
{
    return sqrt(pow(x - CC_X, 2) + pow(y - CC_Y, 2));
}

bool drone::is_fully_charged()
{
    return charge_time <= 0;
}

void drone::charge()
{
    charge_time -= T; // Decrements by timestep T
}

bool drone::is_at_base()
{
    return x == CC_X && y == CC_Y;
}

bool drone::has_reached_range_limit()
{
    return distanceToCC <= 0;
}

// Calculate remaining flight range based on battery percentage
double drone::get_autonomy()
{
    return (battery / 100.0) * MAX_DISTANCE;
}

// Check if drone needs to return for charging
bool drone::is_low_battery()
{
    // Need enough battery for round trip to control center plus safety margin
    return get_autonomy() <= (calculate_distance_to_base() * 2) + MARGIN;
}

void drone::set_last_dist()
{
    distanceToCC = calculate_distance_to_base();
}

void drone::request_drone_replacement(redisContext* c)
{

    //subarea is 200m × 200m
    // msg e.g. type low_battery did 123 cy 20 cx 20 ny 40 nx 40 dx -1 dy -1
    double d_last; // distance the drone still needs to cover to complete its current pattern.
    if (!(f_status == PATTERN_RESET))
    {
        if (job->dy) // If moving downward in the grid (dy = 1)
        {
            // gives number of remaining rows Multiplying by 180 gives total distance including all horizontal movements in those rows
            //the center is chosen as the drone’s position to ensure consistent movement. A cell’s center is offset by 10 meters from the edges
            d_last = (job->say + 190 - y) / 20 * 180; // 20m × 9 = 180m horizontally (9 horizontal movements of 20m each)
            d_last -= (job->ny - y);
        }
        else
            // Moving upward (dy = -1)
        {
            d_last = (y - job->say + 10) / 20 * 180;
            d_last -= (y - job->ny);
        }
    }
    else // When restarting pattern in subarea
    {
        // Only consider direct distance to next target
        if (job->dy)
        {
            d_last = (job->ny - y);
        }
        else
        {
            d_last = (y - job->ny);
        }
    }

    // Add remaining horizontal distance in current row
    if (job->dx)
    {
        d_last += (job->sax + 190 - x); // 190 = 200m - 10m offset
    }
    else
    {
        d_last += (x - job->sax + 10); // +10 for center of first cell
    }


    // Calculate how far drone can go before needing to return
    // distanceToCC: distance to Control Center
    // d_last: distance needed to complete pattern
    int temp = distanceToCC - d_last;


    // Convertire questa distanza in coordinate griglia
    // Using 11 instead of 10 provides a buffer row that triggers reset
    int move_y = (temp / 11) % 11; // Determina la riga nella griglia
    int nexty;

    if (job->dy) //down
    {
        if (move_y > 9) // Beyond grid bounds, reset to top
        {
            nexty = job->say + 10; // Start at first row
        }
        else
        {
            // Calculate position: start + (grid_position * cell_size) + center_offset
            nexty = (int)job->say + move_y * 20 + 10;
        }
    }
    else // Moving up
    {
        if (move_y > 9) // Beyond grid bounds, reset to bottom
        {
            nexty = job->say + 190; // Start at last row
        }
        else
        {
            // Calculate from bottom: start + subarea_height - (grid_position * cell_size)

            nexty = (int)job->say + 190 - move_y * 20;
        }
    }

    int move_x = (temp % 10); // Determina la colonna nella griglia

    //Determinare direzione X basata sulla riga
    int ndx = ((nexty / 20) % 2) ? 1 : -1;
    ndx = job->sax < 3000 ? ndx : -ndx;

    int nextx; // X effettiva basata sulla direzione
    if (!ndx)
    {
        nextx = job->sax + move_x * 20 + 10;
    }
    else
    {
        nextx = job->sax + 190 - move_x * 20;
    }
    redisReply* reply = (redisReply*)redisCommand(c, "XADD %s * type %s did %d say %d sax %d ny %d nx %d dy %d dx %d"
                                                  , "drone_stream", "low_battery", id, job->say, job->sax, nexty
                                                  , nextx, ndx, job->dy); //

    validate_reply_type(c, reply, REDIS_REPLY_STRING);
    freeReplyObject(reply);
    //distanceToCC += 400;
}


// Calculate normalized velocity vector towards destination
void drone::calc_velocity(double targetX, double targetY)
{
    // distance to target x,y
    double distanceToTargetX = targetX - x;
    double distanceToTargetY = targetY - y;


    // Calculate vector length using Pythagoras
    // Normalize the differences to obtain unit vectors
    double totalDistanceToTarget = sqrt(distanceToTargetX * distanceToTargetX + distanceToTargetY * distanceToTargetY);


    //Determine movement speed for this update_position
    // Use smaller of maximum speed or distance to target
    // speed = 20m per update_position (from constructor: speed = (30.0 / 3.6) * T)

    // If target is closer than 20m, update_position exactly that distance
    // Otherwise update_position maximum 20m
    double movementSpeed = (speed >= totalDistanceToTarget) ? totalDistanceToTarget : speed;

    if (totalDistanceToTarget <= 0)
    {
        // Already at target
        velx = 0;
        vely = 0;
        return;
    }
    // Normalize direction vector and scale by speed
    double directionX = distanceToTargetX / totalDistanceToTarget;
    double directionY = distanceToTargetY / totalDistanceToTarget;

    velx = movementSpeed * directionX;
    vely = movementSpeed * directionY;
}

void reset_patrol_pattern(Job* job)
{
    int subarea_side = GRID_SIZE * 10;
    int offset = 10;

    if (job->sax > 3000 && job->say > 3000)
    {
        job->ny = job->say + offset;
        job->nx = job->sax + offset;
        job->dx = 1;
        job->dy = 1;
    }
    else if (job->sax < 3000 && job->say > 3000)
    {
        job->ny = job->say + offset;
        job->nx = job->sax + subarea_side - offset;
        job->dx = -1;
        job->dy = 1;
    }
    else if (job->sax < 3000 && job->say < 3000)
    {
        job->ny = job->say + subarea_side - offset; //size of subarea
        job->nx = job->sax + subarea_side - offset;
        job->dx = -1;

        job->dy = -1;
    }
    else
    {
        job->ny = job->say + subarea_side - offset; //size of subarea
        job->nx = job->sax + offset;
        job->dx = 1;
        job->dy = -1;
    }
}

// Main movement function implementing different flight patterns
void drone::update_position(int t)
{
    // Decrease battery based on time step
    this->battery -= (100.0 / (1800.0 / T));

    // Update distance remaining if waiting for replacement
    if (status == HANDOVER_MODE)
    {
        distanceToCC -= speed; // Counts down the remaining distance it can cover
    }
    switch (this->f_status)
    {
    case PATTERN_RESET:
    case MOVING_TO_TARGET:
        {
            // Direct flight to target coordinates
            this->calc_velocity(job->nx, job->ny);
            //printf("velx : %f, vely : %f\n", velx, vely);
            x += velx;
            y += vely;

            // Check if reached target
            if (x == job->nx && y == job->ny)
            {
                f_status = COVERAGE_PATTERN; // Switch to coverage pattern
                last_v_x = x;
                last_v_y = y;
                last_t_v = t; // mark position verified at time t
            }
            break;
        }
    case RETURN_TO_BASE:
        {
            this->calc_velocity(CC_X,CC_Y);
            x += velx;
            y += vely;
            break;
        }
    case HANDOVER_SCAN:
    case COVERAGE_PATTERN:

        // Calculate step for one update_position in lawn-mower pattern
        double move_distance;
        if (job->dx == -1) // left
        {
            // Calculate distance to left edge or use standard update_position distance
            double distanceToLeftEdge = x - (job->sax + 10);
            move_distance = (speed >= distanceToLeftEdge) ? distanceToLeftEdge : speed;

            this->x += move_distance * job->dx;

            // Verify grid point passage
            if (x <= last_v_x - GRID_SIZE)
            {
                last_v_x = last_v_x - GRID_SIZE;
                last_t_v = t; // position verified at time t
            }
        }
        else
        {
            // Moving right // 190 -> SUBAREA_SIZE - GRID_OFFSET
            move_distance = (speed >= (job->sax + 190) - x) ? ((job->sax + 190) - x) : speed;
            this->x += move_distance * job->dx;
            if (x >= last_v_x + GRID_SIZE)
            {
                last_v_x = last_v_x + GRID_SIZE;
                last_t_v = t; // position verified at time t
            }
        }

        // Check if reached edge of subarea
        if (x == job->sax + 10 || x == job->sax + 190)
        {
            // Move down one row and reverse direction
            job->ny += job->dy * GRID_SIZE;
            job->nx = x;
            job->dx = -job->dx; //reverse direction
            f_status = MOVING_TO_TARGET;

            // Check if subarea is complete, moved beyond subarea boundaries
            if (job->ny < job->say || job->ny > job->say + 200)
            {
                reset_patrol_pattern(this->job);

                if (status != HANDOVER_MODE)
                    f_status = PATTERN_RESET;
            }
        }

        break;
    }
}

void drone::tick(redisContext* c, int t)
{
    redisReply* reply;
    switch (this->status)
    {
    case STARTUP:
        {
            printf("start\n");
            // Send awakening message to control center via drone_stream
            //const char *message_id = "*"; // Send to the latest message in the stream
            const char* stream_name = "drone_stream";
            reply = RedisCommand(c, "XADD %s * type awk did %d", stream_name, id);
            printf("drone_registered msg: ");

            validate_reply_type(c, reply, REDIS_REPLY_STRING);
            freeReplyObject(reply);

            // Transition to READY state after successful registration
            status = READY;
            break;
        }
    case READY:
        {

            // Waiting state - drone checks for assigned tasks


            //printf("drone n: %d\n", id);

            // read drone's personal stream
            reply = (redisReply*)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %d >",
                                              "diameter", "drones", id);
            validate_reply(c, reply);

            // If no messages, remain idle
            if (reply->elements == 0)
            {
                freeReplyObject(reply);
                break;
            }

            this->job = parse_job_from_reply(reply);

            //printf("************job nx: %d, ny : %d, sax : %d, say : %d\n", job->nx, job->ny, job->sax, job->say);

            status = ON_MISSION;
            f_status = MOVING_TO_TARGET;
            freeReplyObject(reply);

            break;
        }
    case ON_MISSION:
        {

            // Active task execution state
            // go to pos nx ny with velx vely

            // if arrived calc next using lawn mowner algo


            // Check if battery is getting too low to complete round trip
            //if current_autonomy <= d_control center x2 + 2margin (max dist :w
            //the drone can get while waiting other drone while keep moving)
            if (this->is_low_battery())
            {
                // calculate last poss for this drone
                // calc next poss for the next drone (should be cur_pos + d_control_cent ofc applied on the grid)
                this->set_last_dist(); // Calculate remaining distance
                this->request_drone_replacement(c); // Request replacement

                // Transition to waiting state
                status = HANDOVER_MODE;
                this->f_status = HANDOVER_SCAN;
                break;
            }
            // Continue normal movement pattern
            this->update_position(t);

            break;
        }
    case HANDOVER_MODE:
        {

            // Waiting for replacement drone while continuing until safe battery range



            if (this->has_reached_range_limit())
            {
                // If finished current segment, prepare to return home
                this->job->nx = CC_X; // Set destination to control center
                this->job->ny = CC_Y;
                status = HOMING;
                this->f_status = RETURN_TO_BASE;
            }
            this->update_position(t); // Continue movement while waiting
            // vai alla pos lastx lasty
            break;
        }
    case HOMING:

        // Returning to control center
        {
            if (this->is_at_base())
            {
                // When reached control center, begin charging
                status = CHARGING;
                this->charge_time = calculate_random_charge_time();
                //printf("charging for : %f\n",charge_time);
                free(this->job); // Clean up completed job
                break;
            }
            this->update_position(t); // Continue movement toward home
            break;
        }
    case CHARGING:

        // Recharging at control center
        {
            this->charge(); // Decrease remaining charge time
            if (this->is_fully_charged())
            {
                // When fully charged, return to idle state
                status = READY;
                this->battery = 100.0;
                //printf("drone %d FULLY CHARGED\n",id);

                const char* stream_name = "drone_stream";
                reply = RedisCommand(c, "XADD %s * type recharged did %d", stream_name, id);
                validate_reply_type(c, reply, REDIS_REPLY_STRING);
                freeReplyObject(reply);
            }
            //printf("drone status: CHARGING\n");
            break;
        }
    default:
        printf("Unknown drone status\n");
        break;
    }
}

void DroneFleet::update_position(int t)
{
    // At each time step, update the state of all drones in the swarm
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        //    printf("+++++++++++++\ndrone : %d, x: %f , y %f, status : %d, f_status : %d, battery : %f, distanceToCC : %f\n",drones[i].id, drones[i].x, drones[i].y, drones[i].status, drones[i].f_status, drones[i].battery,drones[i].distanceToCC);
        //   printf(">>>>> say %d, sax %d, dx %d, dy %d\nny : %d, nx %d\n",drones[i].job->say,drones[i].job->sax,drones[i].job->dx,drones[i].job->dy,drones[i].job->ny,drones[i].job->nx);
        //printf("y : %lf, x : %lf\n",drones[i].y, drones[i].x);

        // Execute state machine update_position for each drone
        drones[i].tick(c, t);
    }
}

void DroneFleet::log()
{
    redisReply* reply;
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        if (drones[i].status == READY)
        {
            continue;
        }
        reply = RedisCommand(c, "XADD %s * did %d battery %f lvy %d lvx %d tv %d status %d"
                                          , log_stream, i, drones[i].battery, drones[i].last_v_y, drones[i].last_v_x
                                          , drones[i].last_t_v, drones[i].status);
        validate_reply(c, reply);
        freeReplyObject(reply);
    }
}

void DroneFleet::shutdown()
{
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        free(drones[i].job);
    }
    redisFree(c);
}
