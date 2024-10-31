#include "ControlCenter.h"
#include "../../con2redis/src/con2redis.h"
#include <vector>
void ControlCenter::print_status() {
    printf("status : %d\n",status);
}
void print_parameters() {
    printf("Horizon : %d\nDrones count: %d\n", HORIZON,DRONES_COUNT);
}
// Implementazione dei metodi della classe ControlCenter
ControlCenter::ControlCenter() {/* Costruttore */}
const char * int_to_string(int x) {
    return std::to_string(x).c_str();
}
//TODO add it to the lib
redisReply* read_stream(redisContext *c, const char *stream_name) {
    // Read from the stream starting from the beginning
    redisReply *reply = (redisReply *)redisCommand(c, "XREAD STREAMS %s 0", stream_name);
    assertReply(c,reply);
    return reply;
}

void ControlCenter::addDrone(Drone drone) {
    // Aggiunge un drone al centro di controllo
    drones.push_back(drone);
}
void ControlCenter::init_grid(){
    for (int i = 0 ; i < HEIGHT; i++) {
            for (int j =0 ; j < WIDTH; j++) {
                grid[i][j] = -1;
            }
    }
}
void ControlCenter::init() {
    //setup conn to redis
    this->pid = getpid();
    init_grid();
    const char *hostname = "127.0.0.1";
    int port = 6379;
    status = STARTUP;
    area_verified = 0;
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
    //cleans results

    std::ofstream outputFile(TEST_PATH); // Open the file for writing (overwrite mode)
    outputFile << "verified_area,t\n"; // Write the string to the file
    outputFile.close();

    //set up sync streams
    redisReply *reply;
    reply = (redisReply *)redisCommand(c, "DEL %s", sync_stream);
    assertReplyType(c, reply, REDIS_REPLY_INTEGER);
        initStreams(c, sync_stream);
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c, "DEL %s", "sync_stream2");
    assertReplyType(c, reply, REDIS_REPLY_INTEGER);
        initStreams(c, "sync_stream2");
    freeReplyObject(reply);

    //set up stream for receiving from drone
    reply = (redisReply *)redisCommand(c, "DEL %s", drone_stream);
    assertReplyType(c, reply, REDIS_REPLY_INTEGER);
        initStreams(c , drone_stream);
    freeReplyObject(reply);
    
    //set up log stream
    reply = (redisReply *)redisCommand(c, "DEL %s", log_stream);
    assertReplyType(c, reply, REDIS_REPLY_INTEGER);
        initStreams(c, log_stream);
    freeReplyObject(reply);

    //set up streams to send to a single drone create one stream for each drone
    for (int i = 0; i< DRONES_COUNT; i++){
    const char *str = int_to_string(i);
    printf("key : %s\n", str);
    reply = (redisReply *)redisCommand(c, "DEL %d", i);
    assertReplyType(c, reply, REDIS_REPLY_INTEGER);
    freeReplyObject(reply);
    //initStreams(c , str);
    reply = RedisCommand(c, "XGROUP CREATE %d diameter $ MKSTREAM", i);
    //dumpReply(reply,0);
    assertReply(c, reply);

    freeReplyObject(reply);
    }


}
int ControlCenter::get_available_drone_id(){
    auto is_available = [](Drone  drone){return (drone.status == IDLE_D);};
    auto drone = std::find_if(drones.begin(),drones.end(), is_available);
    if ( drone != drones.end()){
        return drone->id;
    } else {
        return -1;
    }
}
void ControlCenter::await_sync() {
    redisReply *reply;

    // Send a synchronization message to the Redis stream
    const char *message_id = "*"; // Send to the latest message in the stream
    const char *message = "Sync";
    reply = (redisReply *)redisCommand(c, "XADD %s %s type %s", "sync_stream2", message_id, message);
    if(DEBUG) {
        //dumpReply(reply,0);
    }
    assertReplyType(c, reply,REDIS_REPLY_STRING );
    //if needed log reply later
    freeReplyObject(reply);

    // Block to receive synchronization messages from the Redis stream
    //read sync_stream1
    reply = read_1msg_blocking(c, "diameter", "cc", block_time, sync_stream);
    assertReply(c, reply);
    //dumpReply(reply,0);
    // if needed, dump reply or read it, but shouldnt be necessary atm
    //TODO check wheter the reply contains the sync msg
    freeReplyObject(reply);
    // we could send the time t in the stream or get it calculated from the drones
}
void get_job_msg(Job *my_job, char *buffer ) {
    //TODO remove hardcoded 50
    snprintf(buffer, 50, "sax %d say %d ny %d nx %d dx %d dy %d", 
             my_job->sax, my_job->say, my_job->ny, my_job->nx, my_job->dx, my_job->dy);
}
Job * create_job(int say, int sax, int ny, int nx, int dx, int dy) {
    Job* new_job = (Job*)malloc(sizeof(Job));
    //TODO could be necessary to add dy
    new_job->sax = sax;
    new_job->say = say;
    if (ny < 0|| nx < 0) {
        int subareaside = 20*10;
        if (sax > 3000 && say > 3000) {
            new_job->ny = say + 10;
            new_job->nx = sax + 10;
            new_job->dx = 1;
            new_job->dy = 1;
        } else if (sax < 3000 && say > 3000) {

            new_job->ny = say +10;
            new_job->nx = sax + subareaside -10 ;
            new_job->dx = -1;
            new_job->dy = 1;
        
        } else if (sax < 3000 && say < 3000) {

            new_job->ny = say + subareaside -10; //size of subarea
            new_job->nx = sax + subareaside -  10;
            new_job->dx = -1;
            
            new_job->dy = -1;
        } else {

            new_job->ny = say + subareaside - 10 ; //size of subarea
            new_job->nx = sax + 10 ;
            new_job->dx = 1;
            new_job->dy = -1;
        }
        return new_job;
    }

    new_job->dx = dx;
    new_job->dy = dy;
    new_job->ny = ny;
    new_job->nx = nx;

    return new_job;
}
void ControlCenter::send_substitute(int new_drone_id){
    redisReply * rep;
    Job * job = job_queue.back();

    rep = (redisReply *)redisCommand(c, "XADD %d * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                            , new_drone_id, "task", new_drone_id, job->say,
                                             job->sax, job->ny, job->nx, job->dx, job->dy );
    job_queue.pop_back();
    freeReplyObject(rep);
    this->drones[new_drone_id].job = job;
    this->drones[new_drone_id].status = FLYING_D;
    
}
void ControlCenter::handle_msg(const char * type, redisReply *reply ) {
    int msg_type;
    if (strcmp(type,"low_battery") == 0) {
        msg_type = 0;
    } else if (strcmp(type,"charging") == 0) {
        msg_type = 1;
    } else {// recharged
        msg_type = 2;
    }
    switch(msg_type) {
        case 0: {
        //low_battery msgtype e.g. type low_battery did 123 cy 0 cx 0 nexty 40 nextx 65 dirx -1
        char say [8];
        char sax [8];
        char ny [8];
        char nx [8];
        char dx [8];
        char dy [8];
        char did [8];
        //create job using next coord. and subarea coord.
        ReadStreamMsgVal(reply,0,0,3,did);
        ReadStreamMsgVal(reply,0,0,5,say);
        ReadStreamMsgVal(reply,0,0,7,sax);
        ReadStreamMsgVal(reply,0,0,9,ny);
        ReadStreamMsgVal(reply,0,0,11,nx);
        ReadStreamMsgVal(reply,0,0,13, dx);
        ReadStreamMsgVal(reply,0,0,15, dy);
        Job *job = create_job(std::stoi(say),std::stoi(sax),std::stoi(ny),std::stoi(nx),std::stoi(dx), std::stoi(dy));

        int new_drone_id = get_available_drone_id(); // returns the first free drone
        if ( new_drone_id == -1 ) {
            job_queue.push_back(job);
            free(this->drones[std::stoi(did)].job );
            this->drones[std::stoi(did)].status = HOMING_D;
            return;
        }

        //printf(">>>>>>>>>sending %d drone\n",new_drone_id);
        redisReply * rep;
        //TODOTODOTODO IMPORTANT
        rep = (redisReply *)redisCommand(c, "XADD %d * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                            , new_drone_id, "task", new_drone_id, job->say,
                                             job->sax, job->ny, job->nx, job->dx, job->dy );//job.msg() returns the formated job in a string-type in order to send it via stream
        assertReplyType(c, rep, REDIS_REPLY_STRING);
        //dumpReply(rep,0);

        //chagne new drone status
        this->drones[new_drone_id].job = job;
        this->drones[new_drone_id].status = FLYING_D;
        //change homing drone status
        free(this->drones[std::stoi(did)].job );
        this->drones[std::stoi(did)].status = HOMING_D;
        // change lowe_battery_id status to homing
            break;
        }
        case 1:{
        //charging msgtype e.g. type charging did 123
        char did [4];
        ReadStreamMsgVal(reply,0,0,3,did);
        this->drones[atoi(did)].status = CHARGING_D;

        // change is status to charging
        }
            break;
        case 2: {
        char did [4];
        ReadStreamMsgVal(reply,0,0,3,did);
        this->drones[atoi(did)].status = IDLE_D;
        //recharged msgtype e.g. type recharged did 123
        // change id status to IDLE
            break;
        }
    }
}

void ControlCenter::tick() {
    switch (status) {
        case STARTUP: {
            //startup
            printf("startup\n");
            redisReply *reply ;
            //redisReply *msgs = reply->element[0];// reading from only one stream so [0]

            // get elements as droneid, 
            char value[8];
            for( int i =0 ; i< DRONES_COUNT; i++) {
                //should check type remember later + format
                // first field = type, 2nd drone_id
                /* define msg types later as integers
                    wake = 0 ,

                    */
                // field value example should be 0 , 0
                if(DEBUG) {
                    //printf("drone n: %d\n",i);
                }
                reply = read_1msg_blocking(c,"diameter", "cc",10000, drone_stream);
                assertReply(this->c, reply );
                if(reply->type != REDIS_REPLY_NIL && reply->elements>0){
                    //dumpReply(reply,0);
                    ReadStreamMsgVal(reply, 0, 0, 3, value) ;// reading 3rd element of the msg
                    int drone_id = std::stoi(value);;
                    // add drones to the array
                    this->addDrone(Drone(drone_id)); // constructor with just the id init drone position at start

                }
                freeReplyObject(reply);
            }
            
            if (this->drones.size() == DRONES_COUNT) {//add DRONES COUNT TO CC attr, get it from command line
                this->status = READY;
            }
            break;
        }
        case READY: {
            //ready
            // calcola aree
            /*Sub area should be verifiable in <=5 min with just 1 drone
              since a drone flyes at 30km/h, in 5 min it can cover a distance of
              2.5km or 125 points each 20 m apart
              so we need to calculate 300*300(area with points already 20 m apart)/125
              which are roughly 720 sub areas
              so we need atleast 720 drones + drones for when the battery is over
              
              estimated drones 720 * time of charge / autonomy = 8k drones*/
            // we ve got to keep in mind that the drones which are doing the verify need also to
            //come back to the starter position within the area before the 5 min mark
            // so the subarea must be a square of side x | x^2 + sqrt(2x^2) = 125 
            // x = 10.495 so we round down to 10
            // subareas become then 10 x 10 points spaced 20 m apart
            // Total area gets divided in 900 sub areas of 100 points(20 m apart) 
            // so there are 30 points on each axis in order to obtain the sub areas

            /*   subarea 10x10 points 
                +------+------+------+------+------+(300x300 * (points) 20 m apart)
                |***** |***** |***** |***** |***** |
                |***** |***** |***** |***** |***** |
                |***** |***** |***** |***** |***** |
                +------+------+------+------+------+
                |***** |***** |***** |***** |***** |
                |***** |***** |***** |***** |***** |
                |***** |***** |***** |***** |***** |
                +------+------+------+------+------+
                |***** |***** |***** |***** |***** |
                |***** |***** |***** |***** |***** |
                |***** |***** |***** |***** |***** |
                +------+------+------+------+------+
                (30x30 subareas)
*/
            //every drone gets the top-left coordinate, starting point(sp), and starting direction(sdx)  as [1,-1] only or rigth left  
            // 

            //we identify each subarea with the top-left coordinate and the side of length 10 points

            //we consider at the start the full swarm to be charged and ready
            redisReply *reply;
            for (int i=0; i<30; i++) {
                for(int j =0; j< 30; j++) {
                    //send message to drone this->drones[i*subareasy + subareax] with coord (i,j), sp and sd

                    // job_msg  e.g. y 80 x 50 spy 90 spx 60 sdx -1 (left) 
                    // also add all of this in a job struct to be associated with the drone obj 

                    int did = i *SUB_AREAS_W + j;
                    const char * stream_n = int_to_string( did  );//stream number associated with the drone

                    Job *job = create_job(i * 200,j * 200,-1,-1,-1,-1); // returns a job struct using the subarea coord. use -1 for default

                    reply = (redisReply *)redisCommand(c, "XADD %s * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                            , stream_n, "task", did, job->say,
                                             job->sax, job->ny, job->nx, job->dx, job->dy );//job.msg() returns the formated job in a string-type in order to send it via stream
                    //dumpReply(reply,0);
                    assertReplyType(c, reply, REDIS_REPLY_STRING);

                    freeReplyObject(reply);
                    // change drone status to running
                    this->drones[i*SUB_AREAS_H + j].job = job;
                    this->drones[i*SUB_AREAS_H + j].status = FLYING_D;

                }
            }
            // assegna ogni area ad un drone

            //go to running
            if(DEBUG) printf("changing to running in 3sec\n");
            this->status = RUNNING;
            break;
        }
        case RUNNING: {
            //running
            //read messages

            // once the cc is up and running, it only recevices msgs and responds accordingly to each one
            
            //read all msgs in the stream at time t
            redisReply *reply ;

            char msg_type[30];

            for( int i =0 ; i< DRONES_COUNT; i++) {
                
                //const char * stream_n = int_to_string(i);
                //printf("drone  : %d\n", i);
               // reply = read_1msg(this->c, "diameter", "cc", *stream_n );
                reply = (redisReply *)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >", 
                                    "diameter", "cc",  drone_stream);
                assertReply(c,reply);


                //dumpReply(reply,0);
                if (reply->type == REDIS_REPLY_NIL || reply-> elements ==0 ) {
                    freeReplyObject(reply);
                    continue;
                }
                ReadStreamMsgVal(reply, 0, 0, 1, msg_type) ;// reading 1 == msg type

                handle_msg(msg_type, reply ); // function that handles msgs according to msg type
                freeReplyObject(reply);
            }

            if (job_queue.size() > 0) {
                int new_drone = get_available_drone_id();
                if (new_drone != -1) {
                    send_substitute(new_drone);
                }
            }


            
            break;
        }
        default:
            //no status 
            break;
    }
}
bool is_verified(int last_v, int current_time){
    return last_v != -1 && ((double)(current_time -last_v)/ T) <= (300.0 / T);
}

void ControlCenter::dumpsubarea(int cur){
    printf("err %d\n",grid[54][148]);
    for (int i = 0 ; i < 20; i++) {
        printf("\n" );
        if(i%10 == 0) printf("\n");
            for (int j =0 ; j <  20; j++) {
                if (j % 10 == 0) printf("  ");
                printf("%d  ", grid[i][j]);
            }
    }
}
double ControlCenter::get_percentage(int curr_time){
    int aa = 0;
    for (int i = 0 ; i < HEIGHT; i++) {
            for (int j =0 ; j < WIDTH; j++) {
                if(!is_verified(grid[i][j], curr_time)){

                    //dumpsubarea(curr_time);
                    continue;
                }
                aa ++;
            }
    }
    return ((double)aa/(300.0*300.0))*100.0; 
}
bool ControlCenter::check_area(int curr_time){
    int aa = 0;
    for (int i = 0 ; i < HEIGHT; i++) {
            for (int j =0 ; j < WIDTH; j++) {
                if(!is_verified(grid[i][j], curr_time)){
                    
                    //dumpsubarea(curr_time);
                }
                aa++;
            }
    }
    return ((double)aa/(300.0*300.0))*100.0 >= 99.0; 
}
void save_to_csv(double va, int t , std::string filename) {
    std::ofstream file;
    file.open(filename, std::ios::out | std::ios::app); // Open file in append mode

    if (file.is_open()) {
        file<< va<< "," << t << "\n"; // Write data to the file
        file.close();
    } else {
        std::cerr << "Error: Unable to open file " << filename << " for writing.\n";
        exit(1);
    }
}
void ControlCenter::log(int time) {
    // wait drone statuses for logs bloccante  (this is for the monitor so it can be blocking)
    if(!(status ==RUNNING)) return;
    redisReply * reply;
    int a =0;
    //drone log e.g. did 123 battery 89 lvy 30 lvx 90 tv 340 status 0
    for(int i= 0; i<DRONES_COUNT;i++){
        reply = (redisReply *)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >", 
                                    "diameter", "cc", log_stream);
        //dumpReply(reply,0);
        if (reply->elements == 0) {
            freeReplyObject(reply);
            continue;
        }
        a++;
        char drone_id [8];
        char lvy [8];
        char lvx [8];
        char tv [8];
        char bat [16];
        char stat [2];
        //create job using next coord. and subarea coord.
        //TODO if drone idle continue
        ReadStreamMsgVal(reply,0,0,1,drone_id);
        ReadStreamMsgVal(reply,0,0,3,bat);
        ReadStreamMsgVal(reply,0,0,5,lvy);
        ReadStreamMsgVal(reply,0,0,7,lvx);
        ReadStreamMsgVal(reply,0,0,9,tv);
        ReadStreamMsgVal(reply,0,0,11,stat);

        int did = std::stoi(drone_id);
        if( std::stoi(stat) == 1) { //1 == idle
            freeReplyObject(reply);
            continue;
        }
        int ilvx = std::stoi(lvx);
        int ilvy = std::stoi(lvy);
        ilvx = ilvx/20;
        ilvy = ilvy/20;
        drones[did].battery = std::stod(bat);

        // MONITOR BATTERIA
        assert(drones[did].battery > MIN_BATTERY);
        int ltv = std::stoi(tv);
        
        if (  grid[ilvy][ilvx] < ltv) {
            //printf("changing griddd  y : %d x : %d\n",ilvy,ilvx);
            grid[ilvy][ilvx] = ltv; //setting last_time verified in order to check verif
            drones[did].last_verified_x = ilvx;
            drones[did].last_verified_y = ilvy;
        }

        freeReplyObject(reply);

        // optimize

    }
    //printf("%d drones\n",a);
    bool area_verified1 = area_verified; //verfica allo stato precedente
    area_verified = check_area(time);
    save_to_csv(get_percentage(time), time,TEST_PATH);
    //dumpsubarea(time) ;
    //MONITOR AREA
    assert( area_verified1 ? area_verified: 1); // se l' area diventa verificata non deve mai smettere

    // MONITOR TIME 
    if(!area_verified1 && area_verified){
        assert( MAX_VERIFY_TIME >= time*T); 
    }
    

    //after updating the drones array update the grid based on drone positions in the msg
    
    // get monitors bool val on the data and log on csv file

    // use a csv file no need to use postgresql 
}

// clean up resources by deleting streams and free the context
void ControlCenter::shutdown() {
    redisReply *reply;
    for(int i=0; i<DRONES_COUNT; i++) {
        const char * stream_n = int_to_string(i);
        reply = (redisReply *)RedisCommand(c, "DEL %s", stream_n);
        assertReply(c, reply);
        freeReplyObject(reply);
        free(drones[i].job);
    }

    reply = (redisReply *)redisCommand(c, "DEL %s", drone_stream);
    assertReply(c,reply);
    freeReplyObject(reply);
    
    reply = (redisReply *)redisCommand(c, "DEL %s", sync_stream);
    assertReply(c,reply);
    freeReplyObject(reply);

    redisFree(this-> c);


}

