#ifndef connector

#define connector

#include <stdio.h>
#include <stdlib.h>

#include <hiredis/hiredis.h>


// macros

#define RedisCommand(fmt, ...)			\
(redisReply*) redisCommand(fmt, __VA_ARGS__)


#define dbg_log(fmt, ... )			\
    do { \
        fprintf(stderr, "%s:%d : " fmt "\n", __FILE__, __LINE__,__VA_ARGS__); \
    } while (0);


#define dbg_abort(fmt, ...) \
    do { \
        dbg_log(fmt, __VA_ARGS__); exit(-1); \
    } while (0)



redisReply* stream_publish_message(redisContext* c, char* stream_name, char* message);

redisReply* read_1msg(redisContext* c, const char* group, const char* consumer, const char* stream_name);

redisReply* stream_read_message_blocking(redisContext* c, const char* group, const char* consumer, int block,
                               const char* stream_name);

void validate_reply_type(redisContext* c, redisReply* r, int type);

void validate_reply(redisContext* c, redisReply* r);


void stream_initialize_group(redisContext* c, const char* stream);

void ReadStreamReply(redisReply* r, int indent, int readstatus);

int ReadStreamName(redisReply* r, char* streamname, long unsigned int k);

int ReadNumStreams(redisReply* r);

int ReadStreamNumMsg(redisReply* r, long unsigned int streamnum);

int ReadStreamNumMsgID(redisReply* r, long unsigned int streamnum, int msgnum, char* msgid);

int ReadStreamMsgNumVal(redisReply* r, long unsigned int streamnum, int msgnum);

int get_stream_message_field_value(redisReply* r, long unsigned int streamnum, int msgnum, int entry, char* fval);

#endif
