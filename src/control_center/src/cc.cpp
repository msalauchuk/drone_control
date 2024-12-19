#include "cc.h"
#include "../../redis/src/connector.h"
#include <vector>



cc::cc():
pid(getpid()),
status(STARTUP),
area_verified(0),
c(nullptr)
{
    initialize_grid();
    init_redis();
}


void cc::addDrone(drone drone)
{
    // Aggiunge un drone al centro di controllo
    drones.push_back(drone);
}

void cc::initialize_grid()
{
    std::fill(&grid[0][0], &grid[0][0] + WIDTH * HEIGHT, -1);
}

void cc::init_redis()
{
    /* FASE 1: INIZIALIZZAZIONE VARIABILI BASE */
    // Configurazione connessione Redis
    const char* hostname = "127.0.0.1"; // localhost
    int port = 6379; //  default port


    /* FASE 2: CONNESSIONE A REDIS */
    // Tenta la connessione al server Redis
    c = redisConnect(hostname, port);
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
    reply = (redisReply*)redisCommand(c, "DEL %s", "time_sync");
    validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
    stream_initialize_group(c, "time_sync");
    freeReplyObject(reply);

    // Stream per comunicazioni generali dai droni
    reply = (redisReply*)redisCommand(c, "DEL %s", drone_stream);
    validate_reply_type(c, reply, REDIS_REPLY_INTEGER);
    stream_initialize_group(c, drone_stream);
    freeReplyObject(reply);

    //log_event stream
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
    auto is_available = [](drone drone) { return (drone.status == startup); };
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
    // time_sync: nome dello stream
    // message_id: ID del messaggio ("*" per auto-generazione)
    // type: campo del messaggio
    // message: valore del campo type
    reply = (redisReply*)redisCommand(c, "XADD %s %s type %s", "time_sync", messageId, syncMessage);
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

void serialize_job_msg(Job* my_job, char* buffer)
{
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
    new_job->dx = dx;
    new_job->dy = dy;
    new_job->ny = ny;
    new_job->nx = nx;

    return new_job;
}

void cc::send_substitute_drone(int new_drone_id)
{
    redisReply* rep;
    Job* job = job_queue.back();

    rep = (redisReply*)redisCommand(c, "XADD %d * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                    , new_drone_id, "task", new_drone_id, job->say,
                                    job->sax, job->ny, job->nx, job->dx, job->dy);
    job_queue.pop_back();
    freeReplyObject(rep);
    this->drones[new_drone_id].job = job;  // Assign job to new drone
    this->drones[new_drone_id].status = onMission;
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

            char say[8];
            char sax[8];
            char ny[8];
            char nx[8];
            char dx[8];
            char dy[8];
            char did[8];

            // Estrae i valori dal messaggio Redis e crea job per la subarea

            get_stream_message_field_value(reply, 0, 0, 3, did); // ID del drone con batteria scarica
            get_stream_message_field_value(reply, 0, 0, 5, say); // Coordinate correnti e successive
            get_stream_message_field_value(reply, 0, 0, 7, sax);
            get_stream_message_field_value(reply, 0, 0, 9, ny);
            get_stream_message_field_value(reply, 0, 0, 11, nx);
            get_stream_message_field_value(reply, 0, 0, 13, dx);
            get_stream_message_field_value(reply, 0, 0, 15, dy);


            Job* job = create_job(std::stoi(say), std::stoi(sax), std::stoi(ny), std::stoi(nx), std::stoi(dx),
                                  std::stoi(dy));

            int new_drone_id = get_available_drone_id(); // returns the first free drone
            if (new_drone_id == -1)
            {
                job_queue.push_back(job);

                free(this->drones[std::stoi(did)].job);
                this->drones[std::stoi(did)].status = homing;
                return;
            }

            redisReply* rep;
            rep = (redisReply*)redisCommand(c, "XADD %d * type %s did %d say %d sax %d ny %d nx %d dx %d dy %d"
                                            , new_drone_id, "task", new_drone_id, job->say,
                                            job->sax, job->ny, job->nx, job->dx,
                                            job->dy);
            validate_reply_type(c, rep, REDIS_REPLY_STRING);

            this->drones[new_drone_id].job = job;
            this->drones[new_drone_id].status = onMission;

            // Libera il job del drone scarico e lo manda a ricaricare
            free(this->drones[std::stoi(did)].job);
            this->drones[std::stoi(did)].status = homing;
            break;
        }
    case 1:
        {
            char did[4];
            get_stream_message_field_value(reply, 0, 0, 3, did);
            this->drones[atoi(did)].status = charging;
        }
        break;
    case 2:
        {

            char did[4];
            get_stream_message_field_value(reply, 0, 0, 3, did);
            this->drones[atoi(did)].status = startup;
            break;
        }
    }
}

void cc::process_simulation_step()
{
    switch (status)
    {
    case STARTUP:
        {
            //startup
            printf("startup\n");
            redisReply* reply;
            // get elements as droneid,
            char value[8];
            for (int i = 0; i < DRONES_COUNT; i++)
            {
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
                    get_stream_message_field_value(reply, 0, 0, 3, value); // Get 3rd element (drone_id)
                    int drone_id = std::stoi(value);
                    // Create and add new drone to control center (add drones to the array)
                    this->addDrone(drone(drone_id));
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
            redisReply* reply;
            for (int i = 0; i < 30; i++)
            {
                for (int j = 0; j < 30; j++)
                {

                    int did = i * SUB_AREAS_W + j; // ID del drone che gestirà questa subarea

                    const char* stream_n = int_to_string(did); //stream number associated with the drone

                    Job* job = create_job(i * 200, j * 200, -1, -1, -1, -1);


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
                        );
                    validate_reply_type(c, reply, REDIS_REPLY_STRING);

                    freeReplyObject(reply);
                    // change drone status
                    this->drones[i * SUB_AREAS_H + j].job = job;
                    this->drones[i * SUB_AREAS_H + j].status = onMission;
                }
            }

            if (DEBUG) printf("RUNNING\n");
            this->status = RUNNING;
            break;
        }
    case RUNNING:
        {
            redisReply* reply;

            char msg_type[30];

            for (int i = 0; i < DRONES_COUNT; i++)
            {
                //const char * stream_n = int_to_string(i);
                //printf("drone  : %d\n", i);
                // reply = read_1msg(this->c, "diameter", "cc", *stream_n );



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

                handle_msg(msg_type, reply);
                freeReplyObject(reply);
            }

            if (job_queue.size() > 0)
            {
                int new_drone = get_available_drone_id();
                if (new_drone != -1)
                {
                    send_substitute_drone(new_drone);
                }
            }


            break;
        }
    default:
        break;
    }
}

bool verified_single_point(int last_v, int current_time)
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

double cc::calculate_verified_percentage(int curr_time)
{
    int aa = 0; // Contatore dei punti verificati correttamente
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            if (!verified_single_point(grid[i][j], curr_time))
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
bool cc::verify_area(int curr_time) const
{
    int aa = 0;
    for (int i = 0; i < HEIGHT; i++)
    {
        for (int j = 0; j < WIDTH; j++)
        {
            if (!verified_single_point(grid[i][j], curr_time))
            {
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

void cc::log_event(int time)

{

    // Skip se non in stato RUNNING
    if (!(status == RUNNING))
    {
        printf("Skipping log_event - not in RUNNING state\n");
        return;
    }


    redisReply* reply;
    int a = 0;
    static int last_log_count = 0;


    for (int i = 0; i < DRONES_COUNT; i++)
    {

        reply = (redisReply*)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >",
                                          "diameter", "cc", log_stream);
        if (reply->elements == 0)
        {
            freeReplyObject(reply);
            continue;
        }

        a++;

        char drone_id[8],lvy[8], lvx[8], tv[8], bat[16], stat[2];


        // Estrae informazioni dal messaggio di log_event
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
            freeReplyObject(reply);
            continue;
        }
        int ilvx = std::stoi(lvx);
        int ilvy = std::stoi(lvy);
        ilvx = ilvx / 20;
        ilvy = ilvy / 20;
        drones[did].battery_lvl = std::stod(bat);

        // MONITOR BATTERIA: Verifica che la batteria non scenda sotto il minimo
        assert(drones[did].battery_lvl > MIN_BATTERY);

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


    }
    // // Add summary print
    // if (a != last_log_count) {
    //     printf("Processed %d drone logs at time %d\n", a, time);
    //     last_log_count = a;
    // }
    //printf("%d drones\n",a);
    // MONITOR AREA: Verifica che l'area verificata non diminuisca
    bool area_verified1 = area_verified;
    area_verified = verify_area(time);
    save_to_csv(calculate_verified_percentage(time), time,TEST_PATH);
    //MONITOR AREA
    assert(area_verified1 ? area_verified: 1); // se l' area diventa verificata non deve mai smettere

    // MONITOR TIME: Verifica il tempo massimo per la verifica completa
    if (!area_verified1 && area_verified)
    {
        assert(MAX_VERIFY_TIME >= time*T);
    }


    //after updating the drones array update the grid based on drone positions in the msg

    // get monitors bool val on the data and log_event on csv file

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

void cc::print_status() const
{
    printf("Control Center Status: %d\n", status);
}

void print_simulation_configuration()
{
    printf("Simulation Duration (seconds): %d\n", TOTAL_SIMULATION_TIME);
    printf("Number of Drones: %d\n", DRONES_COUNT);
}
const char* int_to_string(int x)
{
    return std::to_string(x).c_str();
}


