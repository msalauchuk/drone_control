#include "src/redis_connector/include/connector.h"
#include "src/redis_connector/include/local.h"


int ReadNumStreams(redisReply* r)
{
    return (r->elements); // Ritorna il numero totale di streams
}


// reads k-th stream name
int ReadStreamName(redisReply* r, char* streamname, long unsigned int k)
{

    // Copia il nome della k-esima stream
    strcpy(streamname, r->element[k]->element[0]->str);

    return (0);
}

// reads k-th stream name
int ReadStreamNumMsg(redisReply* r, long unsigned int streamnum)
{

    // Ritorna il numero di messaggi nella stream specificata
    return (r->element[streamnum]->element[1]->elements);
}

// k-th stream, i-th msg id
int ReadStreamNumMsgID(redisReply* r, long unsigned int streamnum, int msgnum, char* msgid)
{
    // Legge l'ID del messaggio specificato
    strcpy(msgid, r->element[streamnum]->element[1]->element[msgnum]->element[0]->str);
    return (0);
}


// k-th stream, i-th msg id
int ReadStreamMsgNumVal(redisReply* r, long unsigned int streamnum, int msgnum)
{
    // Ritorna il numero di valori nel messaggio specificato

    return (r->element[streamnum]->element[1]->element[msgnum]->element[1]->elements);
}

// even entry field, odd entry field value
int ReadStreamMsgVal(redisReply* r, long unsigned int streamnum, int msgnum, int entry, char* value)
{

    // Legge un valore specifico dal messaggio
    strcpy(value, r->element[streamnum]->element[1]->element[msgnum]->element[1]->element[entry]->str);
    return (0);
}
