#include "Drone.h"
#include "con2redis.h"
#include <hiredis/hiredis.h>



Drone::Drone(int did)
{
    id = did;
    battery = 100,
        x = CC_X;
    y = CC_Y;
    f_status = FLYING_F;
    speed = (30.0 / 3.6) * T;
    status = STARTUP;
    last_v_x = 0;
    last_v_y = 0;
    last_t_v = -1;
}

void Swarm::addDrone(Drone drone)
{
    // Aggiunge un drone al centro di controllo
    drones.push_back(drone);
}

void Swarm::init_drones(int n)
{
    for (int i = 0; i < n; i++)
    {
        addDrone(Drone(i));
        printf("init id :%d\n", drones[i].id);
    }
}

void Swarm::await_sync()
{
    redisReply* reply;


    // receive synchronization messages from the Redis sync_stream2
    reply = read_1msg_blocking(c, "diameter", "drone", block_time, sync_stream);
    freeReplyObject(reply);

    // we could send the time t in the stream or get it calculated from the drones
    // Send a synchronization message to the Redis sync_stream1
    const char* message = "sync";
    reply = RedisCommand(c, "XADD %s * type %s", "sync_stream1", message);

    if (DEBUG)
    {
        //dumpReply(reply,0);
        assertReplyType(c, reply, REDIS_REPLY_STRING);
    }
    freeReplyObject(reply);
}

void Swarm::init()
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

Job* get_job_from_reply(redisReply* reply)
{
    //low_battery msgtype e.g. type low_battery did 123 cy 0 cx 0 nexty 40 nextx 65 dirx -1 diry -1
    char say[8],sax[8],ny[8],nx[8],dx[8], dy[8];

    ReadStreamMsgVal(reply, 0, 0, 5, say);
    ReadStreamMsgVal(reply, 0, 0, 7, sax);
    ReadStreamMsgVal(reply, 0, 0, 9, ny);
    ReadStreamMsgVal(reply, 0, 0, 11, nx);
    ReadStreamMsgVal(reply, 0, 0, 13, dx);
    ReadStreamMsgVal(reply, 0, 0, 15, dy);

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
double get_random_charge_time()
{
    // Generate a random number between min and max
    double min = 2 * 60 * 60; //2h
    double max = 3 * 60 * 60; //3h
    double randomNumber = (double)rand() / RAND_MAX * (max - min) + min;

    return randomNumber;
}

// Calculate Euclidean distance from drone to control center sqrt(dx² + dy²)
double Drone::get_distance_control_center()
{
    return sqrt(pow(x - CC_X, 2) + pow(y - CC_Y, 2));
}

bool Drone::is_charged()
{
    return charge_time <= 0;
}

void Drone::charge()
{
    charge_time -= T; // Decrements by timestep T
}

bool Drone::is_home()
{
    return x == CC_X && y == CC_Y;
}

bool Drone::is_done()
{
    return last_dist <= 0;
}

// Calculate remaining flight range based on battery percentage
double Drone::get_autonomy()
{
    return (battery / 100.0) * MAX_DISTANCE;
}

// Check if drone needs to return for charging
bool Drone::is_low_battery()
{
    // Need enough battery for round trip to control center plus safety margin
    return get_autonomy() <= (get_distance_control_center() * 2) + MARGIN;
}

void Drone::set_last_dist()
{
    last_dist = get_distance_control_center();
}

void Drone::send_replace_drone_msg(redisContext* c)
{
    // msg e.g. type low_battery did 123 cy 20 cx 20 ny 40 nx 40 dx -1 dy -1
    double d_last;
    if (!(f_status == RESTARTING))
    {
        if (job->dy)
        {
            //we are on the down side of the grid
            d_last = (job->say + 190 - y) / 20 * 180;
            d_last -= (job->ny - y);
        }
        else
        {
            d_last = (y - job->say + 10) / 20 * 180;
            d_last -= (y - job->ny);
        }
    }
    else
    {
        if (job->dy)
        {
            d_last = (job->ny - y);
        }
        else
        {
            d_last = (y - job->ny);
        }
    }
    if (job->dx)
    {
        //bottom right
        d_last += (job->sax + 190 - x);
    }
    else
    {
        d_last += (x - job->sax + 10);
    }
    int temp = last_dist - d_last;

    int move_y = (temp / 11) % 11;
    int nexty;
    if (job->dy)
    {
        if (move_y > 9)
        {
            nexty = job->say + 10;
        }
        else
        {
            nexty = (int)job->say + move_y * 20 + 10;
        }
    }
    else
    {
        if (move_y > 9)
        {
            nexty = job->say + 190;
        }
        else
        {
            nexty = (int)job->say + 190 - move_y * 20;
        }
    }

    int move_x = (temp % 10);

    int ndx = ((nexty / 20) % 2) ? 1 : -1;
    ndx = job->sax < 3000 ? ndx : -ndx;

    int nextx;
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

    assertReplyType(c, reply, REDIS_REPLY_STRING);
    freeReplyObject(reply);
    //last_dist += 400;
}


// Calculate normalized velocity vector towards destination
void Drone::calc_velocity(double destx, double desty)
{
    // Normalize the direction vector
    double diffX = destx - x;
    double diffY = desty - y;


    // Calculate vector length
    // Normalize the differences to obtain unit vectors
    double length = sqrt(diffX * diffX + diffY * diffY);


    // Use smaller of maximum speed or distance to target
    double max_speed = (speed >= length) ? length : speed;

    if (length <= 0)
    {
        velx = 0;
        vely = 0;
        return;
    }
    // Normalize direction vector and scale by speed
    double unitX = diffX / length;
    double unitY = diffY / length;
    velx = max_speed * unitX;
    vely = max_speed * unitY;
}

void reset_job(Job* job)
{
    int subareaside = 20 * 10;
    if (job->sax > 3000 && job->say > 3000)
    {
        job->ny = job->say + 10;
        job->nx = job->sax + 10;
        job->dx = 1;
        job->dy = 1;
    }
    else if (job->sax < 3000 && job->say > 3000)
    {
        job->ny = job->say + 10;
        job->nx = job->sax + subareaside - 10;
        job->dx = -1;
        job->dy = 1;
    }
    else if (job->sax < 3000 && job->say < 3000)
    {
        job->ny = job->say + subareaside - 10; //size of subarea
        job->nx = job->sax + subareaside - 10;
        job->dx = -1;

        job->dy = -1;
    }
    else
    {
        job->ny = job->say + subareaside - 10; //size of subarea
        job->nx = job->sax + 10;
        job->dx = 1;
        job->dy = -1;
    }
}

// Main movement function implementing different flight patterns
void Drone::move(int t)
{
    // Decrease battery based on time step
    this->battery -= (100.0 / (1800.0 / T));

    // Update distance remaining if waiting for replacement
    if (status == WAIT_NEXT_DRONE)
    {
        last_dist -= speed;
    }
    switch (this->f_status)
    {
    case RESTARTING:
    case FLYING_F:
        {
            // Direct flight to target coordinates
            this->calc_velocity(job->nx, job->ny);
            //printf("velx : %f, vely : %f\n", velx, vely);
            x += velx;
            y += vely;

            // Check if reached target
            if (x == job->nx && y == job->ny)
            {
                f_status = LAWN_MOWNER_F; // Switch to coverage pattern
                last_v_x = x;
                last_v_y = y;
                last_t_v = t; // mark position verified at time t
            }
            break;
        }
    case HOMING_F:
        {
            this->calc_velocity(CC_X,CC_Y);
            x += velx;
            y += vely;
            break;
        }
    case WAIT_NEXT_DRONE_F:
    case LAWN_MOWNER_F:

        // Implement lawn mower pattern for area coverage
        // Calculate appropriate speed based on direction and remaining distance
        double m_speed;
        if (job->dx == -1)
        {
            // Moving left
            m_speed = (speed >= x - (job->sax + 10)) ? (x - (job->sax + 10)) : speed;
            this->x += m_speed * job->dx;
            if (x <= last_v_x - 20)
            {
                last_v_x = last_v_x - 20;
                last_t_v = t; // position verified at time t
            }
        }
        else
        {
            // Moving right
            m_speed = (speed >= (job->sax + 190) - x) ? ((job->sax + 190) - x) : speed;
            this->x += m_speed * job->dx;
            if (x >= last_v_x + 20)
            {
                last_v_x = last_v_x + 20;
                last_t_v = t; // position verified at time t
            }
        }

        // Check if reached edge of subarea
        if (x == job->sax + 10 || x == job->sax + 190)
        {
            // Move to next row and reverse direction
            job->ny += job->dy * 20;
            job->nx = x;
            job->dx = -job->dx;
            f_status = FLYING_F;

            // Check if subarea is complete
            if (job->ny < job->say || job->ny > job->say + 200)
            {
                reset_job(this->job);
                if (status != WAIT_NEXT_DRONE)
                    f_status = RESTARTING;
            }
        }

        break;
    }
}

void Drone::tick(redisContext* c, int t)
{
    redisReply* reply;
    switch (this->status)
    {
    case STARTUP:
        {
            printf("startup\n");
            // Send awakening message to control center via drone_stream
            //const char *message_id = "*"; // Send to the latest message in the stream
            const char* stream_name = "drone_stream";
            reply = RedisCommand(c, "XADD %s * type awk did %d", stream_name, id);
            printf("awk msg: ");

            assertReplyType(c, reply, REDIS_REPLY_STRING);
            freeReplyObject(reply);

            // Transition to IDLE state after successful registration
            status = IDLE;
            break;
        }
    case IDLE:
        {

            // Waiting state - drone checks for assigned tasks


            //printf("drone n: %d\n", id);

            // read drone's personal stream
            reply = (redisReply*)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %d >",
                                              "diameter", "drones", id);
            assertReply(c, reply);

            // If no messages, remain idle
            if (reply->elements == 0)
            {
                freeReplyObject(reply);
                break;
            }

            this->job = get_job_from_reply(reply);

            //printf("************job nx: %d, ny : %d, sax : %d, say : %d\n", job->nx, job->ny, job->sax, job->say);

            status = FLYING;
            f_status = FLYING_F;
            freeReplyObject(reply);

            break;
        }
    case FLYING:
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
                this->send_replace_drone_msg(c); // Request replacement

                // Transition to waiting state
                status = WAIT_NEXT_DRONE;
                this->f_status = WAIT_NEXT_DRONE_F;
                break;
            }
            // Continue normal movement pattern
            this->move(t);


            break;
        }
    case WAIT_NEXT_DRONE:
        {

            // Waiting for replacement drone while continuing limited movement



            if (this->is_done())
            {
                // If finished current segment, prepare to return home
                this->job->nx = CC_X; // Set destination to control center
                this->job->ny = CC_Y;
                status = HOMING;
                this->f_status = HOMING_F;
            }
            this->move(t); // Continue movement while waiting
            // vai alla pos lastx lasty
            break;
        }
    case HOMING:

        // Returning to control center
        {
            if (this->is_home())
            {
                // When reached control center, begin charging
                status = CHARGING;
                this->charge_time = get_random_charge_time();
                //printf("charging for : %f\n",charge_time);
                free(this->job); // Clean up completed job
                break;
            }
            this->move(t); // Continue movement toward home
            break;
        }
    case CHARGING:

        // Recharging at control center
        {
            this->charge(); // Decrease remaining charge time
            if (this->is_charged())
            {
                // When fully charged, return to idle state
                status = IDLE;
                this->battery = 100.0;
                //printf("drone %d FULLY CHARGED\n",id);

                const char* stream_name = "drone_stream";
                reply = RedisCommand(c, "XADD %s * type recharged did %d", stream_name, id);
                assertReplyType(c, reply, REDIS_REPLY_STRING);
                freeReplyObject(reply);
            }
            //printf("Drone status: CHARGING\n");
            break;
        }
    default:
        printf("Unknown drone status\n");
        break;
    }
}

void Swarm::tick(int t)
{
    // At each time step, update the state of all drones in the swarm
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        //    printf("+++++++++++++\ndrone : %d, x: %f , y %f, status : %d, f_status : %d, battery : %f, last_dist : %f\n",drones[i].id, drones[i].x, drones[i].y, drones[i].status, drones[i].f_status, drones[i].battery,drones[i].last_dist);
        //   printf(">>>>> say %d, sax %d, dx %d, dy %d\nny : %d, nx %d\n",drones[i].job->say,drones[i].job->sax,drones[i].job->dx,drones[i].job->dy,drones[i].job->ny,drones[i].job->nx);
        //printf("y : %lf, x : %lf\n",drones[i].y, drones[i].x);

        // Execute state machine tick for each drone
        drones[i].tick(c, t);
    }
}

void Swarm::log()
{
    redisReply* reply;
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        if (drones[i].status == IDLE)
        {
            continue;
        }
        reply = (redisReply*)RedisCommand(c, "XADD %s * did %d battery %f lvy %d lvx %d tv %d status %d"
                                          , log_stream, i, drones[i].battery, drones[i].last_v_y, drones[i].last_v_x
                                          , drones[i].last_t_v, drones[i].status);
        assertReply(c, reply);
        freeReplyObject(reply);
    }
}

void Swarm::shutdown()
{
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        free(drones[i].job);
    }
    redisFree(c);
}
