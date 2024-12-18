#include "connector.h"
#include "debug_config.h"





// Validates that a Redis reply is of the expected type
// Aborts if reply is NULL or doesn't match expected type

void validate_reply_type(redisContext* c, redisReply* r, int type)
{
    if (r == NULL)
        dbg_abort("NULL redisReply (error: %s)", c->errstr);

    //printf("Got reply type: %d, Expected type: %d\n", r->type, type);


    if (r->type == REDIS_REPLY_ERROR) {
        printf("Redis Error: %s\n", r->str);
    }

    if (r->type != type)
    {
        print_reply_types();
        dbg_abort("Expected reply type %d but got type %d", type, r->type);
    }
}


// Simple validation that a Redis reply exists
// Aborts if reply is NULL

void validate_reply(redisContext* c, redisReply* r)
{
    if (r == NULL)
        dbg_abort("NULL redisReply (error: %s)", c->errstr);
}


// Reads a single message from a Redis stream using consumer groups
// Non-blocking read

redisReply* read_1msg(redisContext* c, const char* group, const char* consumer, const char* stream_name)
{
    redisReply* reply = (redisReply*)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >",
                                                  group, // Consumer group name
                                                  consumer, // Consumer name within the group
                                                  stream_name // Stream to read from
                                                  );
    validate_reply(c, reply);
    return reply;
}

// Reads a single message from a Redis stream using consumer groups
// Blocking read with timeout

redisReply* stream_read_message_blocking(redisContext* c, const char* group, const char* consumer, int block,
                               const char* stream_name)
{
    redisReply* reply = (redisReply*)redisCommand(
        c,
        "XREADGROUP GROUP %s %s COUNT 1 BLOCK %d NOACK STREAMS %s >",
        group,
        consumer,
        block, // Time to block/wait in milliseconds
        stream_name

    );

    validate_reply(c, reply);
    return reply;
}


// Sends a message to a Redis stream

redisReply* stream_publish_message(redisContext* c, char* stream_name, char* message)
{
    redisReply* reply = (redisReply*)redisCommand(
        c,
        "XADD %s * type %s", // Add entry to stream with auto-generated ID (*)
        stream_name, // Stream to write to
        message // Message content
    );

    validate_reply(c, reply);
    return reply;
}

void stream_initialize_group(redisContext* c, const char* stream)

{

    redisReply* r = RedisCommand(c,
        "XGROUP CREATE %s diameter $ MKSTREAM",
        stream);

    if (r != NULL) {
        // Handle successful creation (OK) or BUSYGROUP error
        if (r->type == REDIS_REPLY_STATUS ||
           (r->type == REDIS_REPLY_ERROR && strstr(r->str, "BUSYGROUP"))) {
            freeReplyObject(r);
            return;
           }

        // If we get here, something unexpected happened
        printf("Unexpected reply type %d for stream %s\n", r->type, stream);
    }

    validate_reply(c, r);
    freeReplyObject(r);
}


void print_reply_types()
{
    printf("REDIS_REPLY_STRING=%d,\
REDIS_REPLY_STATUS=%d,\
REDIS_REPLY_INTEGER=%d,\
REDIS_REPLY_NIL=%d,\
REDIS_REPLY_ERROR=%d,\
REDIS_REPLY_ARRAY=%d\n",
           REDIS_REPLY_STRING,
           REDIS_REPLY_STATUS,
           REDIS_REPLY_INTEGER,
           REDIS_REPLY_NIL,
           REDIS_REPLY_ERROR,
           REDIS_REPLY_ARRAY
    );
}
