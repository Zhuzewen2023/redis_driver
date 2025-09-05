#include <stdio.h>
#include <string.h>
#include <hiredis/hiredis.h>

static inline int check_reply(redisReply* r, char* command)
{
	if (r == NULL || r->type == REDIS_REPLY_ERROR) {
		printf("Command: %s failed: %s\n", command, (r ? r->str : "unknown error"));
		if (r) freeReplyObject(r);
		return -1;
	}
	if (r->type == REDIS_REPLY_STRING) {
		printf("Command: %s success: %s\n", command, r->str);
	}
	else if (r->type == REDIS_REPLY_INTEGER) {
		printf("Command: %s success: %lld\n", command, r->integer);
	}
	else if (r->type == REDIS_REPLY_ARRAY) {
		printf("Command: %s success, array number: %lu\n", command, r->elements);
	}
	return 0;
}

void cleanup(redisContext* c, redisReply* r)
{
	if (r) freeReplyObject(r);
	if (c) redisFree(c);
}

int string_operation(redisContext* c)
{
	redisReply* reply = NULL;
	char command[512] = { 0 };
	snprintf(command, sizeof(command), "SET str:name 'z2w-redis-demo'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	printf("SET %s: %s\n", command, reply->str);
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "GET str:name");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_STRING) {
		printf("GET %s : %s\n", command, reply->str);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "INCR str:counter");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("GET %s : %lld\n", command, reply->integer);
	}
	freeReplyObject(reply);

	return 0;

}

int hash_operation(redisContext* c)
{
	redisReply* reply = NULL;
	char command[512] = { 0 };
	snprintf(command, sizeof(command), "HMSET hash:user id 100 name 'z2w' age 27");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_STRING) {
		printf("GET %s : %s\n", command, reply->str);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "HGET hash:user name");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_STRING) {
		printf("HGET %s : %s\n", command, reply->str);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "HGETALL hash:user");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_ARRAY) {
		printf("HGETALL %s success, elements: %lu\n", command, reply->elements);
		for (size_t i = 0; i < reply->elements; i += 2) {
			printf(" %s : %s\n", reply->element[i]->str, reply->element[i + 1]->str);
		}
	}
	freeReplyObject(reply);

	return 0;
}

int list_operation(redisContext* c)
{
	redisReply* reply = NULL;
	char command[512] = { 0 };
	snprintf(command, sizeof(command), "LPUSH list:fruits 'apple' 'banana'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("LPUSH result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "RPUSH list:fruits 'mango' 'watermelon'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("RPUSH result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "LRANGE list:fruits 0 -1");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_ARRAY) {
		printf("LRANGE success, elements: %lu\n", reply->elements);
		for (size_t i = 0; i < reply->elements; i ++) {
			printf(" %lu : %s\n", i, reply->element[i]->str);
		}
	}
	freeReplyObject(reply);

	return 0;
}

int set_operation(redisContext* c)
{
	redisReply* reply = NULL;
	char command[512] = { 0 };
	snprintf(command, sizeof(command), "SADD set:tags 'c' 'c++' 'python'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("SADD result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "SISMEMBER set:tags 'c++'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("SISMEMBER result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "SREM set:tags 'python'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("SREM result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);
	return 0;
}

int zset_operation(redisContext* c)
{
	redisReply* reply = NULL;
	char command[512] = { 0 };
	snprintf(command, sizeof(command), "ZADD zset:ranks 90 'alice' 85 'bob' 95 'charlie'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("ZADD result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "ZRANGE zset:ranks 0 -1 WITHSCORES");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_ARRAY) {
		printf("ZRANGE WITHSCORES result: %lu\n", reply->elements);
		for (size_t i = 0; i < reply->elements; i += 2) {
			printf(" %s : %s\n", reply->element[i]->str, reply->element[i + 1]->str);
		}
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "ZRANGE zset:ranks 0 -1");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_ARRAY) {
		printf("ZRANGE result: %lu\n", reply->elements);
		for (size_t i = 0; i < reply->elements; i ++) {
			printf(" %s\n", reply->element[i]->str);
		}
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "ZRANK zset:ranks 'bob'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("ZRANK 'bob': %lld\n", reply->integer);
	}
	freeReplyObject(reply);

	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "ZREM zset:ranks 'bob'");
	reply = redisCommand(c, command);
	if (check_reply(reply, command)) {
		return 1;
	}
	if (reply->type == REDIS_REPLY_INTEGER) {
		printf("ZREM result: %lld\n", reply->integer);
	}
	freeReplyObject(reply);
	return 0;
}

int pipeline_operation(redisContext* c)
{
	redisReply* reply = NULL;
	redisAppendCommand(c, "SET pipe:key 'pipeline-test'");
	redisAppendCommand(c, "INCR pipe:counter");
	redisAppendCommand(c, "GET pipe:key");

	if (redisGetReply(c, (void**)&reply) != REDIS_OK) {
		printf("pipeline 1 failed\n");
		return 1;
	}
	printf("pipeline 1 success: %s\n", reply->str);
	freeReplyObject(reply);

	if (redisGetReply(c, (void**)&reply) != REDIS_OK) {
		printf("pipeline 2 failed\n");
		return 1;
	}
	printf("pipeline 2 success: %lld\n", reply->integer);
	freeReplyObject(reply);

	if (redisGetReply(c, (void**)&reply) != REDIS_OK) {
		printf("pipeline 3 failed\n");
		return 1;
	}
	printf("pipeline 3 success: %s\n", reply->str);
	freeReplyObject(reply);

	return 0;
}

int transaction_operation(redisContext* c)
{
	redisReply* reply = NULL;
	reply = redisCommand(c, "MULTI");
	if (check_reply(reply, "MULTI")) {
		return 1;
	}
	printf("MULTI : %s\n", reply->str);
	freeReplyObject(reply);

	char command[512] = { 0 };
	snprintf(command, sizeof(command), "SET trans:key 'transaction-test'");
	redisCommand(c, command);
	memset(command, 0, sizeof(command));
	snprintf(command, sizeof(command), "INCR trans:counter");
	redisCommand(c, command);

	reply = redisCommand(c, "EXEC");
	check_reply(reply, "EXEC");
	if (reply->type == REDIS_REPLY_ARRAY) {
		printf("transaction result: %lu\n", reply->elements);
		for (size_t i = 0; i < reply->elements; i++) {
			redisReply* cmd_reply = reply->element[i];
			if (cmd_reply->type == REDIS_REPLY_STATUS) {
				printf("command %d : %s\n", (int)(i + 1), cmd_reply->str);
			}
			else if (cmd_reply->type == REDIS_REPLY_INTEGER) {
				printf("command %d : %lld\n", (int)(i + 1), cmd_reply->integer);
			}
		}
	}
	freeReplyObject(reply);

	return 0;
}

int main(int argc, char* argv[])
{
	const char* hostname = "127.0.0.1";
	int port = 6379;
	struct timeval timeout = { 1, 500000 };

	redisContext* c = redisConnectWithTimeout(hostname, port, timeout);
	if (c == NULL || c->err) {
		if (c) {
			printf("redis connect failed: %s\n", c->errstr);
			redisFree(c);
		}
		else {
			printf("redis connect failed: redisContext create failed\n");
		}
		return 1;
	}
	printf("Connect to redis %s:%d success\n", hostname, port);

	printf("\n==== string operation ====\n");
	if (string_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	printf("\n==== hash operation ====\n");
	if (hash_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	printf("\n==== list operation ====\n");
	if (list_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	printf("\n==== set operation ====\n");
	if (set_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	printf("\n==== zset operation ====\n");
	if (zset_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	printf("\n==== pipeline operation ====\n");
	if (pipeline_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	printf("\n==== transaction operation ====\n");
	if (transaction_operation(c)) {
		cleanup(c, NULL);
		return 1;
	}

	redisFree(c);
	printf("All operations finished, connection closed\n");

	return 0;
}