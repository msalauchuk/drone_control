#include "cc.h"
#include "../../redis/src/connector.h"
#include <vector>

void cc::print_status()
{
    printf("status : %d\n", status);
}

void print_parameters()
{
    printf("Horizon : %d\nDrones count: %d\n", HORIZON,DRONES_COUNT);
}

// Implementazione dei metodi della classe cc
cc::cc()
{
    /* Costruttore */
}

const char* int_to_string(int x)
{
    return std::to_string(x).c_str();
}

redisReply* read_stream(redisContext* c, const char* stream_name)
{
    // Read from the stream starting from the beginning
    redisReply* reply = (redisReply*)redisCommand(c, "XREAD STREAMS %s 0", stream_name);
    validate_reply(c, reply);
    return reply;
}

void cc::addDrone(drone drone)
{
    // Aggiunge un drone al centro di controllo
    drones.push_back(drone);
}

void cc::init_grid()
{
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            grid[i][j] = -1;
        }
    }
}

void cc::init()
{
    /* FASE 1: INIZIALIZZAZIONE VARIABILI BASE */
    this-> pid = getpid();
    init_grid();
    // Configurazione connessione Redis
    const char* hostname = "127.0.0.1"; // localhost
    int port = 6379; //  default port

    status = STARTUP;
    area_verified = 0;

    /* FASE 2: CONNESSIONE A REDIS */
    // Tenta la connessione al server Redis
    this->c = redisConnect(hostname, port);
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


    /* FASE 3: INIZIALIZZAZIONE FILE DI LOG */
    // Crea/sovrascrive il file di test per i risultati
    std::ofstream outputFile(TEST_PATH); // Open the file for writing (overwrite mode)

    // Scrive header del CSV
    outputFile << "verified_area,t\n"; // Write the string to the file
    outputFile.close();

    /* FASE 4: SETUP STREAMS DI SINCRONIZZAZIONE */
    redisReply* reply;

    // Stream per risposte dai droni
    reply = (redisReply*)redisCommand(c, "DEL %s", drone_ack_stream);
    validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
    stream_initialize_group(c, drone_ack_stream);
    freeReplyObject(reply);

    // Stream per messaggi dal centro di controllo
    reply = (redisReply*)redisCommand(c, "DEL %s", "sync_stream2");
    validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
    stream_initialize_group(c, "sync_stream2");
    freeReplyObject(reply);

    // Stream per comunicazioni generali dai droni
    reply = (redisReply*)redisCommand(c, "DEL %s", drone_stream);
    validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
    stream_initialize_group(c, drone_stream);
    freeReplyObject(reply);

    //log stream
    reply = (redisReply*)redisCommand(c, "DEL %s", log_stream);
    validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
    stream_initialize_group(c, log_stream);
    freeReplyObject(reply);

    /* FASE 5: SETUP STREAMS INDIVIDUALI PER OGNI DRONE */
    // Crea uno stream dedicato per ogni drone
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        const char* str = int_to_string(i);
        printf("key : %s\n", str);

        // Elimina stream esistente se presente
        reply = (redisReply*)redisCommand(c, "DEL %d", i);
        validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
        freeReplyObject(reply);


        // Crea nuovo stream con gruppo consumer
        reply = RedisCommand(c, "XGROUP CREATE %d diameter $ MKSTREAM", i);
        validate_reply(c, reply);
        freeReplyObject(reply);
    }
}

int cc::get_available_drone_id()
{
    auto is_available = [](drone drone) { return (drone.status == IDLE_D); };
    auto drone = std::find_if(drones.begin(), drones.end(), is_available);
    if (drone != drones.end())
    {
        return drone->id;
    }
    else
    {
        return -1;
    }
}

void cc::sync_and_wait()
{
    redisReply* reply;

    /* FASE 1: INVIO MESSAGGIO DI SINCRONIZZAZIONE */


    // "*" indica a Redis di generare automaticamente un ID per il messaggio
    const char* messageId = "*";
    // Messaggio di sincronizzazione semplice
    const char* syncMessage = "Sync";

    // Esegue comando Redis XADD per aggiungere messaggio allo stream
    // sync_stream2: nome dello stream
    // message_id: ID del messaggio ("*" per auto-generazione)
    // type: campo del messaggio
    // message: valore del campo type
    reply = (redisReply*)redisCommand(c, "XADD %s %s type %s", "sync_stream2", messageId, syncMessage);
    if (DEBUG)
    {
        //dumpReply(reply,0);
    }

    // Verifica che la risposta sia di tipo stringa (conferma corretta scrittura)
    validate_reply_type(c, reply,REDIS_REPLY_STRING);

    // Libera memoria allocata per la risposta
    freeReplyObject(reply);


    /* FASE 2: ATTESA CONFERMA */

    // Legge un messaggio in modo bloccante dallo stream
    // diameter: nome gruppo consumatori
    // cc: ID consumatore
    // syncTimeout: tempo massimo di attesa

    //# Lettura bloccante (CC attende risposta)
    reply = stream_read_message_blocking(c, "diameter", "cc", syncTimeout, drone_ack_stream);

    // Verifica validità della risposta
    validate_reply(c, reply);
    //dumpReply(reply,0);
    freeReplyObject(reply);
}

void get_job_msg(Job* my_job, char* buffer)
{
    //TODO remove hardcoded 50
    snprintf(buffer, 50, "sax %d say %d ny %d nx %d dx %d dy %d",
             my_job->sax, my_job->say, my_job->ny, my_job->nx, my_job->dx, my_job->dy);
}

Job* create_job(int say, int sax, int ny, int nx, int dx, int dy)
{
    Job* new_job = (Job*)malloc(sizeof(Job));


    // The initial coordinates
    new_job->sax = sax;
    new_job->say = say;


    // Se ny o nx sono -1, calcola automaticamente il percorso del drone
    if (ny < 0 || nx < 0)
    {

        // Dimensione del lato della subarea (200m = 20m * 10 punti)
        int subareaside = 20 * 10;

        // Calcola direzione e punto successivo in base alla posizione della subarea
        // L'area totale è 6000x6000m (300x300 punti a 20m di distanza)
        // Divisa in 4 quadranti con centro in (3000,3000)



        // Q1: BASSO DESTRA
        if (sax > 3000 && say > 3000)
        {
            new_job->ny = say + 10;
            new_job->nx = sax + 10;
            new_job->dx = 1;
            new_job->dy = 1;
        }

        // Q2: BASSO SINISTRA
        else if (sax < 3000 && say > 3000)
        {
            new_job->ny = say + 10;
            new_job->nx = sax + subareaside - 10;
            new_job->dx = -1;
            new_job->dy = 1;
        }

        // Q3: ALTO SINISTRA
        else if (sax < 3000 && say < 3000)
        {
            new_job->ny = say + subareaside - 10;
            new_job->nx = sax + subareaside - 10;
            new_job->dx = -1;

            new_job->dy = -1;
        }
        else
            // Q4: ALTO DESTRA

        {
            new_job->ny = say + subareaside - 10;
            new_job->nx = sax + 10;
            new_job->dx = 1;
            new_job->dy = -1;
        }
        return new_job;
    }
    // Se sono specificate le coordinate successive, usa quelle
    new_job->dx = dx;
    new_job->dy = dy;
    new_job->ny = ny;
    new_job->nx = nx;

    return new_job;
}

void cc::send_substitute(int new_drone_id)
{
    redisReply* rep;
    Job* job = job_queue.back();

    rep = (redisReply*)redisCommand(c, "XADD %d * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                    , new_drone_id, "task", new_drone_id, job->say,
                                    job->sax, job->ny, job->nx, job->dx, job->dy);
    job_queue.pop_back();
    freeReplyObject(rep);
    this->drones[new_drone_id].job = job;  // Assign job to new drone
    this->drones[new_drone_id].status = FLYING_D;
}

/**
 * Gestisce i messaggi ricevuti dai droni relativi allo stato della batteria
 * @param type Tipo di messaggio ricevuto ("low_battery", "charging", "recharged")
 * @param reply Risposta Redis contenente i dettagli del messaggio
 */

void cc::handle_msg(const char* type, redisReply* reply)
{

    // enum BatteryMsgType {
    //     LOW_BATTERY = 0,
    //     CHARGING = 1,
    //     RECHARGED = 2
    // };
    //
    // BatteryMsgType msg_type;
    // if (strcmp(type, "low_battery") == 0) {
    //     msg_type = LOW_BATTERY;
    // } else if (strcmp(type, "charging") == 0) {
    //     msg_type = CHARGING;
    // } else {
    //     msg_type = RECHARGED;
    // }



    int msg_type;
    if (strcmp(type, "low_battery") == 0)
    {
        msg_type = 0;
    }
    else if (strcmp(type, "charging") == 0)
    {
        msg_type = 1;
    }
    else
    {
        // recharged
        msg_type = 2;
    }
    switch (msg_type)
    {
    case 0:
        {
            // Gestisce la situazione di batteria scarica e assegna il task a un nuovo drone
            // Formato messaggio: type low_battery did 123 cy 0 cx 0 nexty 40 nextx 65 dirx -1 diry 1
            //low_battery msgtype e.g. type low_battery did 123 cy 0 cx 0 nexty 40 nextx 65 dirx -1
            char say[8];
            char sax[8];
            char ny[8];
            char nx[8];
            char dx[8];
            char dy[8];
            char did[8];

            // Estrae i valori dal messaggio Redis
            //create job using next coord. and subarea coord.
            get_stream_message_field_value(reply, 0, 0, 3, did); // ID del drone con batteria scarica
            get_stream_message_field_value(reply, 0, 0, 5, say); // Coordinate correnti e successive
            get_stream_message_field_value(reply, 0, 0, 7, sax);
            get_stream_message_field_value(reply, 0, 0, 9, ny);
            get_stream_message_field_value(reply, 0, 0, 11, nx);
            get_stream_message_field_value(reply, 0, 0, 13, dx);
            get_stream_message_field_value(reply, 0, 0, 15, dy);


            // Crea un nuovo job con le coordinate attuali e successive
            Job* job = create_job(std::stoi(say), std::stoi(sax), std::stoi(ny), std::stoi(nx), std::stoi(dx),
                                  std::stoi(dy));

            // Cerca un drone disponibile per il task
            int new_drone_id = get_available_drone_id(); // returns the first free drone
            if (new_drone_id == -1)
            {
                // Se non ci sono droni disponibili, mette il job in coda
                job_queue.push_back(job);

                // Libera il job del drone con batteria scarica e lo manda a ricaricare
                free(this->drones[std::stoi(did)].job);
                this->drones[std::stoi(did)].status = HOMING_D;
                return;
            }

            // Invia il task al nuovo drone tramite il suo stream dedicato
            //printf(">>>>>>>>>sending %d drone\n",new_drone_id);
            redisReply* rep;
            rep = (redisReply*)redisCommand(c, "XADD %d * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                            , new_drone_id, "task", new_drone_id, job->say,
                                            job->sax, job->ny, job->nx, job->dx,
                                            job->dy);
            validate_reply_type(c, rep, REDIS_REPLY_STRING);

            // Aggiorna lo stato dei droni
            // Assegna il job al nuovo drone e lo mette in volo
            this->drones[new_drone_id].job = job;
            this->drones[new_drone_id].status = FLYING_D;

            // Libera il job del drone scarico e lo manda a ricaricare
            free(this->drones[std::stoi(did)].job);
            this->drones[std::stoi(did)].status = HOMING_D;
            // change lowe_battery_id status to homing
            break;
        }
    case 1:
        {
            // Gestisce l'inizio della ricarica di un drone
            // Formato messaggio: type charging did 123
            char did[4];
            get_stream_message_field_value(reply, 0, 0, 3, did);
            this->drones[atoi(did)].status = CHARGING_D;

            // change is status to charging
        }
        break;
    case 2:
        {

            // Gestisce il completamento della ricarica di un drone
            // Formato messaggio: type recharged did 123
            char did[4];
            get_stream_message_field_value(reply, 0, 0, 3, did);
            this->drones[atoi(did)].status = IDLE_D;
            // change id status to IDLE
            break;
        }
    }
}

void cc::tick()
{
    switch (status)
    {
    case STARTUP:
        {
            //startup
            printf("startup\n");
            redisReply* reply;
            //redisReply *msgs = reply->element[0];// reading from only one stream so [0]

            // get elements as droneid,
            char value[8];
            for (int i = 0; i < DRONES_COUNT; i++)
            {
                //should check type remember later + format
                // first field = type, 2nd drone_id
                /* define msg types later as integers
                    wake = 0 ,

                    */
                // field value example should be 0 , 0
                if (DEBUG)
                {
                    //printf("drone n: %d\n",i);
                }

                // Wait for each drone to send registration message

                reply = stream_read_message_blocking(c, "diameter", "cc", 10000, drone_stream);

                // Blocks for up to 10 seconds waiting for message


                validate_reply(this->c, reply);
                if (reply->type != REDIS_REPLY_NIL && reply->elements > 0)
                {
                    // Message format expected: type wake did {drone_id}
                    get_stream_message_field_value(reply, 0, 0, 3, value); // Get 3rd element (drone_id)
                    int drone_id = std::stoi(value); // Convert to integer
                    // Create and add new drone to control center (add drones to the array)
                    this->addDrone(drone(drone_id)); // constructor with just the id init drone position at start
                }
                freeReplyObject(reply);
            }

            if (this->drones.size() == DRONES_COUNT)
            {
                this->status = READY; // Move to READY state when all drones registered
            }
            break;
        }
    case READY:
        {
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
            redisReply* reply;
            for (int i = 0; i < 30; i++)
            {
                for (int j = 0; j < 30; j++)
                {
                    //send message to drone this->drones[i*subareasy + subareax] with coord (i,j), sp and sd

                    // job_msg  e.g. y 80 x 50 spy 90 spx 60 sdx -1 (left)
                    // also add all of this in a job struct to be associated with the drone obj

                    int did = i * SUB_AREAS_W + j; // ID del drone che gestirà questa subarea

                    const char* stream_n = int_to_string(did); //stream number associated with the drone

                    Job* job = create_job(i * 200, j * 200, -1, -1, -1, -1);
                    // returns a job struct using the subarea coord. use -1 for default
                    // i * 200: coordinata Y iniziale della subarea
                    // j * 200: coordinata X iniziale della subarea
                    // -1, -1, -1, -1: valori default per ny, nx, dx, dy



                    // invio job al drone
                    reply = (redisReply*)redisCommand(
                        c, "XADD %s * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d",
                        stream_n, // Stream specifico per questo drone
                        "task", // Tipo messaggio
                        did,
                        job->say, // Coordinata Y subarea
                        job->sax,
                        job->ny, // Next Y point
                        job->nx,
                        job->dx, // Direzione X
                        job->dy
                        ); //job.msg() returns the formated job in a string-type in order to send it via stream
                    //dumpReply(reply,0);
                    validate_reply_type(c, reply, REDIS_REPLY_STRING);

                    freeReplyObject(reply);
                    // change drone status to running
                    this->drones[i * SUB_AREAS_H + j].job = job; // Assegna job al drone
                    this->drones[i * SUB_AREAS_H + j].status = FLYING_D; // Imposta stato a "in volo"
                }
            }

            if (DEBUG) printf("changing to running in 3sec\n");
            this->status = RUNNING;
            break;
        }
    case RUNNING:
        {
            //running
            //read messages

            // once the cc is up and running, it only receives msgs and responds accordingly to each one

            //read all msgs in the stream at time t
            redisReply* reply;

            char msg_type[30];

            for (int i = 0; i < DRONES_COUNT; i++)
            {
                //const char * stream_n = int_to_string(i);
                //printf("drone  : %d\n", i);
                // reply = read_1msg(this->c, "diameter", "cc", *stream_n );


                // monitora continuamente i messaggi dai droni

                reply = (redisReply*)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >",
                                                  "diameter",
                                                  "cc",
                                                  drone_stream);
                validate_reply(c, reply);


                if (reply->type == REDIS_REPLY_NIL || reply->elements == 0)
                {
                    freeReplyObject(reply);
                    continue; // Se non ci sono messaggi, passa al prossimo drone
                }
                get_stream_message_field_value(reply, 0, 0, 1, msg_type); // reading 1 == msg type

                // handle low_battery/charging/recharged
                handle_msg(msg_type, reply);
                freeReplyObject(reply);
            }

            if (job_queue.size() > 0)
            {
                int new_drone = get_available_drone_id();
                if (new_drone != -1)
                {
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

bool is_verified(int last_v, int current_time)
{
    // - true se il punto è stato verificato negli ultimi 5 minuti (300 secondi)
    // - false se il punto non è mai stato verificato o se la verifica è troppo vecchia
    return last_v != -1 && ((double)(current_time - last_v) / T) <= (300.0 / T);
}

void cc::dumpsubarea(int cur)
{
    printf("err %d\n", grid[54][148]);
    for (int i = 0; i < 20; i++)
    {
        printf("\n");
        if (i % 10 == 0) printf("\n");
        for (int j = 0; j < 20; j++)
        {
            if (j % 10 == 0) printf("  ");
            printf("%d  ", grid[i][j]);
        }
    }
}


// Calcola la percentuale di area totale che è stata verificata correttamente

double cc::get_percentage(int curr_time)
{
    int aa = 0; // Contatore dei punti verificati correttamente
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            if (!is_verified(grid[i][j], curr_time))
            {
                //dumpsubarea(curr_time);
                continue;
            }
            aa++;
        }
    }

    // - aa: numero di punti verificati
    // - 300.0 * 300.0: numero totale di punti nella griglia
    // - * 100.0: conversione in percentuale
    return ((double)aa / (300.0 * 300.0)) * 100.0;
}


// Verifica se l'area totale ha raggiunto la soglia minima di copertura (99%)
bool cc::check_area(int curr_time)
{
    int aa = 0;
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            if (!is_verified(grid[i][j], curr_time))
            {
                //dumpsubarea(curr_time);
                continue;
            }
            aa++;
        }
    }
    return ((double)aa / (300.0 * 300.0)) * 100.0 >= 99.0;
}

void save_to_csv(double va, int t, std::string filename)
{
    std::ofstream file;
    file.open(filename, std::ios::out | std::ios::app); // Open file in append mode

    if (file.is_open())
    {
        file << va << "," << t << "\n"; // Write data to the file
        file.close();
    }
    else
    {
        std::cerr << "Error: Unable to open file " << filename << " for writing.\n";
        exit(1);
    }
}

void cc::log(int time)

{

    // wait drone statuses for logs bloccante  (this is for the monitor so it can be blocking)
    // Skip se non in stato RUNNING
    if (!(status == RUNNING))
    {
        printf("Skipping log - not in RUNNING state\n");
        return;
    }


    redisReply* reply;
    int a = 0;
    static int last_log_count = 0;


    //drone log e.g. did 123 battery 89 lvy 30 lvx 90 tv 340 status 0
    for (int i = 0; i < DRONES_COUNT; i++)
    {

        // Legge messaggi dal log_stream
        reply = (redisReply*)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >",
                                          "diameter", "cc", log_stream);
        //dumpReply(reply,0);
        if (reply->elements == 0)
        {
            freeReplyObject(reply);
            continue;
        }

        a++;

        char drone_id[8],lvy[8], lvx[8], tv[8], bat[16], stat[2];

        //create job using next coord. and subarea coord.

        // Estrae informazioni dal messaggio di log

        get_stream_message_field_value(reply, 0, 0, 1, drone_id);
        get_stream_message_field_value(reply, 0, 0, 3, bat);
        get_stream_message_field_value(reply, 0, 0, 5, lvy);
        get_stream_message_field_value(reply, 0, 0, 7, lvx);
        get_stream_message_field_value(reply, 0, 0, 9, tv);
        get_stream_message_field_value(reply, 0, 0, 11, stat);

        // MONITOR: Verifica delle condizioni critiche
        int did = std::stoi(drone_id);
        if (std::stoi(stat) == 1)
        {
            //1 == idle
            freeReplyObject(reply);
            continue;
        }
        int ilvx = std::stoi(lvx);
        int ilvy = std::stoi(lvy);
        ilvx = ilvx / 20;
        ilvy = ilvy / 20;
        drones[did].battery = std::stod(bat);

        // MONITOR BATTERIA: Verifica che la batteria non scenda sotto il minimo
        assert(drones[did].battery > MIN_BATTERY);

        // Aggiorna la griglia con le ultime verifiche
        int ltv = std::stoi(tv);
        if (grid[ilvy][ilvx] < ltv)
        {
            //printf("Grid updated at [%d,%d] time:%d\n", ilvy, ilvx, ltv);  // Add this
            //printf("changing griddd  y : %d x : %d\n",ilvy,ilvx);
            grid[ilvy][ilvx] = ltv; //setting last_time verified in order to check verif
            drones[did].last_verified_x = ilvx;
            drones[did].last_verified_y = ilvy;
        }


        freeReplyObject(reply);

        // optimize
    }
    // // Add summary print
    // if (a != last_log_count) {
    //     printf("Processed %d drone logs at time %d\n", a, time);
    //     last_log_count = a;
    // }
    //printf("%d drones\n",a);
    // MONITOR AREA: Verifica che l'area verificata non diminuisca
    bool area_verified1 = area_verified; //verfica allo stato precedente
    area_verified = check_area(time);
    save_to_csv(get_percentage(time), time,TEST_PATH);
    //dumpsubarea(time) ;
    //MONITOR AREA
    assert(area_verified1 ? area_verified: 1); // se l' area diventa verificata non deve mai smettere

    // MONITOR TIME: Verifica il tempo massimo per la verifica completa
    if (!area_verified1 && area_verified)
    {
        assert(MAX_VERIFY_TIME >= time*T);
    }


    //after updating the drones array update the grid based on drone positions in the msg

    // get monitors bool val on the data and log on csv file

    // use a csv file no need to use postgresql
}

// clean up resources by deleting streams and free the context
void cc::shutdown()
{
    redisReply* reply;
    for (int i = 0; i < DRONES_COUNT; i++)
    {
        const char* stream_n = int_to_string(i);
        reply = (redisReply*)RedisCommand(c, "DEL %s", stream_n);
        validate_reply(c, reply);
        freeReplyObject(reply);
        free(drones[i].job);
    }

    reply = (redisReply*)redisCommand(c, "DEL %s", drone_stream);
    validate_reply(c, reply);
    freeReplyObject(reply);

    reply = (redisReply*)redisCommand(c, "DEL %s", drone_ack_stream);
    validate_reply(c, reply);
    freeReplyObject(reply);

    redisFree(this->c);
}
