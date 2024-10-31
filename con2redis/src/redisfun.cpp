#include "con2redis.h"
#include "local.h"

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


void assertReplyType(redisContext *c, redisReply *r, int type) {
    if (r == NULL)
        dbg_abort("NULL redisReply (error: %s)", c->errstr);
    if (r->type != type)
      { print_reply_types();
	dbg_abort("Expected reply type %d but got type %d", type, r->type);
      }
}



void assertReply(redisContext *c, redisReply *r) {
    if (r == NULL)
        dbg_abort("NULL redisReply (error: %s)", c->errstr);
}






redisReply* read_1msg(redisContext *c, const char * group, const char* consumer, const char *stream_name) {

    redisReply *reply = (redisReply *)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 NOACK STREAMS %s >", 
                                    group, consumer, stream_name);
    assertReply(c,reply);
    return reply;
}
redisReply* read_1msg_blocking(redisContext *c, const char * group, const char* consumer, int block, const char *stream_name) {

    redisReply *reply = (redisReply *)redisCommand(c, "XREADGROUP GROUP %s %s COUNT 1 BLOCK %d NOACK STREAMS %s >", 
                                    group, consumer, block, stream_name);
    assertReply(c,reply);
    return reply;
}

redisReply* send_redis_msg(redisContext *c , char * stream_name, char * message){
    redisReply * reply = (redisReply*)redisCommand(c, "XADD %s * type %s", stream_name, message);
    assertReply(c,reply);
    return reply;
}
void initStreams(redisContext *c, const char *stream) {
    redisReply *r = RedisCommand(c, "XGROUP CREATE %s diameter $ MKSTREAM", stream);
    assertReply(c, r);
    freeReplyObject(r);
}
