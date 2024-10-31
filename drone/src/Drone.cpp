#include "Drone.h"
#include "../con2redis/src/con2redis.h"
#include <hiredis.h>
const char *int_to_string(int x) {
    return std::to_string(x).c_str();
}


// Implementazione dei metodi della classe Drone
Drone::Drone(int did) {
    id = did;
    battery = 100, 
    x= CC_X;
    y= CC_Y;
    f_status = FLYING_F;
    speed = (30.0/3.6) * T;
    status = STARTUP;
    last_v_x = 0;
    last_v_y = 0;
    last_t_v = -1;
 }
void Swarm::addDrone(Drone drone) {
    // Aggiunge un drone al centro di controllo
    drones.push_back(drone);
}
void Swarm::init_drones(int n) {
    for (int i =0; i<n ; i++){
        addDrone(Drone(i));
        printf("init id :%d\n",drones[i].id);
    }
}
void Swarm::await_sync() {
    redisReply *reply;


    // Block to receive synchronization messages from the Redis stream
    //reply = (redisReply *)RedisCommand(c, "XREAD COUNT 1 BLOCK %d STREAMS %s >", block_time, sync_stream);
    //read syncstream2
    reply = read_1msg_blocking(c, "diameter", "drone",block_time, sync_stream);
    //dumpReply(reply,0);
    
    // if needed, dump reply or read it, but shouldnt be necessary atm
    //TODO check wheter the reply contains the sync msg
    //freeReplyObject(reply);
    // we could send the time t in the stream or get it calculated from the drones

    // Send a synchronization message to the Redis stream
    //char *message_id = "*"; // Send to the latest message in the stream
    const char *message = "sync";
    reply = (redisReply *) RedisCommand(c, "XADD %s * type %s", "sync_stream1", message);
    //reply = (redisReply *)redisCommand(c, "XADD %s %s type %s", sync_stream, message_id, message);
    if(DEBUG) {
        //dumpReply(reply,0);
        assertReplyType(c, reply, REDIS_REPLY_STRING);
    }
    //if needed log reply later
    freeReplyObject(reply);
}
void Swarm::init() {
    //set up redis context
    this->pid = getpid();
    srand( time(NULL));
    const char *hostname = "127.0.0.1";
    int port = 6379;
    this->c = redisConnect(hostname,port);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }
    if(DEBUG) {
        printf("initialized redis con\n");
        redisReply *reply = (redisReply*)redisCommand(c, "PING");
        if (reply == NULL) {
            printf("Error: Failed to execute PING command\n");
            redisFree(c);
            exit(1);
        }

        // Check the reply
        if (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "PONG") == 0) {
            printf("Redis is alive!\n");
        } else {
            printf("Error: Redis did not respond properly\n");
        }

        freeReplyObject(reply);
    }
}
Job * get_job_from_reply(redisReply *reply){
    //low_battery msgtype e.g. type low_battery did 123 cy 0 cx 0 nexty 40 nextx 65 dirx -1 diry -1
        char say [8];
        char sax [8];
        char ny [8];
        char nx [8];
        char dx [8];
        char dy [8];
        //create job using next coord. and subarea coord.
        ReadStreamMsgVal(reply,0,0,5,say);
        ReadStreamMsgVal(reply,0,0,7,sax);
        ReadStreamMsgVal(reply,0,0,9,ny);
        ReadStreamMsgVal(reply,0,0,11,nx);
        ReadStreamMsgVal(reply,0,0,13, dx);
        ReadStreamMsgVal(reply,0,0,15, dy);

        Job *job = (Job *)malloc(sizeof(Job));

        // Assign values to the members
        job->sax = std::stoi(sax);
        job->say =  std::stoi(say);
        job->ny = std::stoi(ny);
        job->nx = std::stoi(nx);
        job->dx = std::stoi(dx);
        job->dy = std::stoi(dy); 
        return job;
}
double get_random_charge_time() {
    // Generate a random number between min and max
    double min = 2 * 60 *60;
    double max = 3 *60 * 60 ;
    double randomNumber = (double)rand() / RAND_MAX * (max - min) + min;
    
    return randomNumber;
}
double Drone::get_distance_control_center(){
    return sqrt(pow(x-CC_X,2) + pow(y-CC_Y,2));
}
bool Drone::is_charged(){
    return charge_time <= 0;
}
void Drone::charge(){
    charge_time -= T; 
}
bool Drone::is_home() {
    return x == CC_X && y == CC_Y;
}
bool Drone::is_done() {
    return last_dist <= 0;
}
double Drone::get_autonomy(){
    return (battery /100.0) * MAX_DISTANCE;
}
bool Drone::is_low_battery(){
    return get_autonomy() <= (get_distance_control_center() *2) + MARGIN;//TODO implement get_dist
}
void Drone::set_last_dist() {
    last_dist = get_distance_control_center();
}

void Drone::send_replace_drone_msg(redisContext * c) {
    // msg e.g. type low_battery did 123 cy 20 cx 20 ny 40 nx 40 dx -1 dy -1
    double d_last;
    if(!(f_status == RESTARTING)){
    if(job->dy) { //we are on the down side of the grid
        d_last = (  job->say +190 - y)/20 *180 ;
        d_last -= (job->ny -y);
    } else {
        d_last = (y - job->say+10)/20 *180;
        d_last -= (y - job->ny);
    }
    } else {
        if (job->dy) {
            d_last = (job->ny -y);
        }else {

            d_last = (y - job->ny);
        }
    }
    if (job->dx){ //bottom right
        d_last += ( job->sax+190 - x);
    } else {
        d_last += (x - job->sax +10);
    }
    int temp = last_dist -d_last;

    int move_y = (temp/11) %11;
    int nexty;
    if (job->dy) {
        if(move_y > 9) {
            nexty = job->say +10;
        }else {
            nexty =(int) job->say + move_y *20 + 10;
        }
    } else {
        if(move_y > 9) {
            nexty = job->say +190;
        }else {
            nexty =(int) job->say +190 - move_y *20  ; 
        }
    }

    int move_x = (temp % 10);

    int ndx = ((nexty /20)%2) ? 1:-1;
    ndx = job->sax<3000? ndx : -ndx;

    int nextx;
    if(!ndx) {
        nextx = job->sax + move_x *20 +10;
    } else {
        nextx = job->sax + 190 - move_x *20;
    }
    redisReply * reply = (redisReply *)redisCommand(c, "XADD %s * type %s did %d say %d sax %d ny %d nx %d dy %d dx %d"
                                    , "drone_stream", "low_battery", id, job->say, job->sax, nexty 
                                    , nextx, ndx, job->dy  );//
    
    assertReplyType(c, reply, REDIS_REPLY_STRING);
    freeReplyObject(reply);
    //last_dist += 400;
}
void Drone::calc_velocity(double destx, double desty) {

    // Normalize the direction vector
    double diffX = destx - x;
    double diffY = desty - y;
    

    // Normalize the differences to obtain unit vectors
    double length = sqrt(diffX * diffX + diffY * diffY); // Length of the vector

    double max_speed = (speed >= length)? length : speed;

    if (length <= 0) {
        velx = 0;
        vely = 0;
        return;
    }
    double unitX = diffX / length;
    double unitY = diffY / length;

    // Calculate velocity components
    velx = max_speed * unitX;
    vely = max_speed * unitY;
}
void reset_job(Job * job) {
    int subareaside = 20*10;
        if (job->sax > 3000 && job->say > 3000) {
            job->ny = job->say + 10;
            job->nx = job->sax+10;
            job->dx = 1;
            job->dy = 1;
        } else if (job->sax < 3000 && job->say > 3000) {

            job->ny = job->say+ 10;
            job->nx = job->sax + subareaside -10 ;
            job->dx = -1;
            job->dy = 1;
        
        } else if (job->sax < 3000 && job->say < 3000) {

            job->ny = job->say + subareaside -10; //size of subarea
            job->nx = job->sax + subareaside -10;
            job->dx = -1;
            
            job->dy = -1;
        } else {

            job->ny = job->say + subareaside -10; //size of subarea
            job->nx = job->sax +10;
            job->dx = 1;
            job->dy = -1;
        }
}
void Drone::move(int t) {
    this ->battery -= (100.0/(1800.0/T)); //TODO 
    if(status == WAIT_NEXT_DRONE) {
                //last_dist -= m_speed;
                last_dist -= speed;
            }
    switch(this->f_status) {
        case RESTARTING:
        case FLYING_F:{
            this->calc_velocity(job->nx, job->ny); 
            //printf("velx : %f, vely : %f\n", velx, vely);
            x += velx;
            y += vely;
            if (x == job->nx && y == job->ny ) {
                f_status = LAWN_MOWNER_F;
                last_v_x = x;
                last_v_y = y;
                last_t_v =t; // position verified at time t
            }
            break;
        }
        case HOMING_F: {
            this->calc_velocity(CC_X,CC_Y ); 
            x += velx;
            y += vely;
            break;
        }
        case WAIT_NEXT_DRONE_F:
        case LAWN_MOWNER_F:
            double m_speed;
            if (job->dx == -1) {
                m_speed = (speed >= x - (job->sax+10) )? (x-(job->sax +10)) : speed;
                this->x += m_speed * job->dx;
                if(x <= last_v_x - 20){
                    last_v_x = last_v_x -20;
                    last_t_v =t; // position verified at time t
                }
            } else {
                m_speed = (speed >= (job->sax +190)  - x)? ((job->sax +190) -x) : speed;
                this->x += m_speed * job->dx;
                if(x >= last_v_x + 20){
                    last_v_x = last_v_x +20;
                    last_t_v =t; // position verified at time t
                }
            }
            
            if(x == job->sax + 10 || x == job->sax + 190) {
                job->ny += job->dy * 20;
                job->nx = x;
                job->dx = -job->dx;
                //fix sta merda
                f_status = FLYING_F;
                if(job->ny < job->say || job->ny > job->say +200) {
                    reset_job(this->job);
                    if ( status != WAIT_NEXT_DRONE)
                    f_status = RESTARTING;
                }
            }

        break;
    }
    
    
}
void Drone::tick(redisContext *c, int t) {
    redisReply *reply;
    switch(this->status) {
        case STARTUP: {
            printf("startup\n");
            //send awake msg
            //const char *message_id = "*"; // Send to the latest message in the stream
            const char * stream_name = "drone_stream";

            reply = (redisReply*) RedisCommand(c, "XADD %s * type awk did %d", stream_name, id);
            printf("awk msg: ");
            //dumpReply(reply,0);
            assertReplyType(c, reply, REDIS_REPLY_STRING);

            freeReplyObject(reply);

            status = IDLE;
            break;
        }
        case IDLE: {
            
            //const char *buf = int_to_string(id ) ;//TODO lib remember to add
            //reply = read_1msg(c, "diameter", buf ,buf); 

            //printf("drone n: %d\n", id);
            reply = (redisReply *)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %d >", 
                                    "diameter", "drones", id);
            assertReply(c, reply );
            //dumpReply(reply,0);
            if (reply-> elements ==0) {
                freeReplyObject(reply);
                break;
            }

            this->job = get_job_from_reply(reply);
            //printf("************job nx: %d, ny : %d, sax : %d, say : %d\n", job->nx, job->ny, job->sax, job->say);
            status = FLYING;
            f_status = FLYING_F;
            freeReplyObject(reply);
            //wait job
            //if job go FLYING
            break;
        }
        case FLYING: {
            // vai alla pos nx ny with velx vely

            // if arrived calc next using lawn mowner algo

            //if current_autonomy <= d_control center x2 + 2margin (max dist :w
            //the drone can get while waiting other drone while keep moving)
            if(this->is_low_battery()) {
            // calculate last poss for this drone
            // calc next poss for the next drone (should be cur_pos + d_control_cent ofc applied on the grid)
            this->set_last_dist();
            this->send_replace_drone_msg(c);
            status= WAIT_NEXT_DRONE;
            this->f_status = WAIT_NEXT_DRONE_F;
            break;
            }

            this->move(t); // if flying do smthing else if waitnext drone do another thing


            break;
        }
        case WAIT_NEXT_DRONE: {//add to f status
            if(this->is_done()) {
                this->job->nx = CC_X;
                this->job->ny = CC_Y;
                status = HOMING;
                this->f_status = HOMING_F;
                //send msg
            }
            this->move(t); 
            // vai alla pos lastx lasty
            break;
        }
        case HOMING:{
            // move home
            if(this-> is_home()) {
                status = CHARGING;
                this-> charge_time = get_random_charge_time();
                //printf("charging for : %f\n",charge_time);
                free(this->job);
                break;
            }
            this-> move(t);

            // if home go charging
            // send msg
            break;
        }
        case CHARGING: {
            //charge
            this->charge();
            if (this->is_charged()) {
                status = IDLE;
                this->battery = 100.0;
                //printf("drone %d FULLY CHARGED\n",id);
                //send mesg
                const char * stream_name = "drone_stream";

                reply = (redisReply*) RedisCommand(c, "XADD %s * type recharged did %d", stream_name, id);
            }
            //if charged go idle
            // send msg
            //printf("Drone status: CHARGING\n");
            break;
        }
        default:
            printf("Unknown drone status\n");
            break;
    }
}
void Swarm::tick(int t) { //performs tick for all drones
    for(int i=0; i < DRONES_COUNT; i++ ) {

        //    printf("+++++++++++++\ndrone : %d, x: %f , y %f, status : %d, f_status : %d, battery : %f, last_dist : %f\n",drones[i].id, drones[i].x, drones[i].y, drones[i].status, drones[i].f_status, drones[i].battery,drones[i].last_dist);
         //   printf(">>>>> say %d, sax %d, dx %d, dy %d\nny : %d, nx %d\n",drones[i].job->say,drones[i].job->sax,drones[i].job->dx,drones[i].job->dy,drones[i].job->ny,drones[i].job->nx);
        //printf("y : %lf, x : %lf\n",drones[i].y, drones[i].x);
        drones[i].tick(c,t);
    }
}
void Swarm::log(){
    redisReply * reply;
    for(int i=0; i < DRONES_COUNT; i++ ) {
        if(drones[i].status == IDLE) {
            continue;
        }
        reply = (redisReply*) RedisCommand(c, "XADD %s * did %d battery %f lvy %d lvx %d tv %d status %d"
                        , log_stream, i, drones[i].battery, drones[i].last_v_y,drones[i].last_v_x
                        ,drones[i].last_t_v, drones[i].status );
        assertReply(c,reply);
        freeReplyObject(reply);
    }
}
void Swarm::shutdown() {
    for(int i =0; i< DRONES_COUNT; i++) {
        free(drones[i].job);
    }
    redisFree(c);
}