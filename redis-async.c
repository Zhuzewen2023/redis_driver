#include "reactor.h"
#include <signal.h>
#include <stdarg.h>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>

reactor_t* g_reactor = NULL;

static void sigint_handler(int sig)
{
	if (g_reactor) {
		printf("Received Ctrl + C , stopping Reactor...\n");
		stop_eventloop(g_reactor);
	}
}

static void redis_async_read_cb(int fd, int events, event_t* e)
{
	redisAsyncContext* ac = (redisAsyncContext*)e->priv;
	if (!ac || ac->c.fd != fd) {
		printf("redisAysncContext is NULL or incorrect fd\n");
		del_event(e->r, e);
		close(fd);
		return;
	}

	redisAsyncHandleRead(ac);
	if (ac->err) {
		printf("Redis async read error: %s\n", ac->errstr);
		del_event(e->r, e);
		redisAsyncFree(ac);
		close(fd);
	}
}

static void redis_async_write_cb(int fd, int events, event_t* e)
{
	redisAsyncContext* ac = (redisAsyncContext*)e->priv;
	if (!ac || ac->c.fd != fd) {
		printf("redisAysncContext is NULL or incorrect fd\n");
		del_event(e->r, e);
		close(fd);
		return;
	}

	redisAsyncHandleWrite(ac);
	if (ac->err) {
		printf("Redis async write error: %s\n", ac->errstr);
		del_event(e->r, e);
		redisAsyncFree(ac);
		close(fd);
	}
}

static void redis_async_error_cb(int fd, char* err)
{
	printf("Redis async error : %s (fd=%d)\n", err, fd);
}

// reactor.c 或 main.c 中，函数外定义普通回调函数
// 注意：函数签名必须严格匹配 hiredis 的回调类型！
// hiredis 定义的连接回调类型：typedef void (*redisConnectCallback)(const redisAsyncContext*, int);
static void redis_async_connect_cb(const redisAsyncContext* ac, int status)
{
	if (status != REDIS_OK) {
		printf("Redis connect failed: %s\n", ac->errstr);
		return;
	}
	printf("Redis async connected successfully (fd = %d)\n", ac->c.fd);
}

static void redis_async_disconnect_cb(const redisAsyncContext* ac, int status)
{
	if (status != REDIS_OK) {
		printf("Redis disconnect error: %s\n", ac->errstr);
	}
	else {
		printf("Redis async disconnected (fd = %d)\n", ac->c.fd);
	}
	redisAsyncContext* ctx = (redisAsyncContext*)ac;
	redisAsyncFree(ctx);
}

event_t* reactor_redis_async_connect(reactor_t* r, const char* host, int port)
{
	redisAsyncContext* ac = redisAsyncConnect(host, port);
	if (ac == NULL || ac->err) {
		const char* err = ac ? ac->errstr : "Failed to allocate async context";
		printf("Redis async connect error: %s\n", err);
		if (ac) redisAsyncFree(ac);
		return NULL;
	}

	redisAsyncSetConnectCallback(ac, redis_async_connect_cb);
	redisAsyncSetDisconnectCallback(ac, redis_async_disconnect_cb);

	event_t* e = new_event(r, ac->c.fd, redis_async_read_cb, redis_async_write_cb, redis_async_error_cb);
	if (!e) {
		redisAsyncFree(ac);
		return NULL;
	}

	e->priv = ac;
	add_event(r, EPOLLIN | EPOLLOUT | EPOLLET, e);

	return e;
}

void reactor_redis_async_send_cmd(event_t* e, redisCallbackFn* cb, void* privdata, const char* fmt, ...)
{
	redisAsyncContext* ac = (redisAsyncContext*)e->priv;
	if (!ac || ac->err) {
		printf("Redis async context invalid\n");
		return;
	}

	va_list ap;
	va_start(ap, fmt);
	redisvAsyncCommand(ac, cb, privdata, fmt, ap);
	va_end(ap);
}

static void redis_string_set_cb(redisAsyncContext* ac, void* reply, void* privdata)
{
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r || r->type == REDIS_REPLY_ERROR) {
		printf("SET %s failed : %s\n", key, r ? r->str : "unknown error");
		return;
	}
	printf("SET %s success : %s\n", key, r->str);
}

// String 命令回调：GET
static void redis_string_get_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r) {
		printf("GET %s failed: unknown error\n", key);
		return;
	}
	if (r->type == REDIS_REPLY_ERROR) {
		printf("GET %s failed: %s\n", key, r->str);
		return;
	}
	if (r->type == REDIS_REPLY_NIL) {
		printf("GET %s: key not exists\n", key);
		return;
	}
	printf("GET %s success: %s\n", key, r->str);
}

// String 命令回调：INCR
static void redis_string_incr_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r || r->type != REDIS_REPLY_INTEGER) {
		printf("INCR %s failed: %s\n", key, r ? r->str : "unknown error");
		return;
	}
	printf("INCR %s success: %lld\n", key, r->integer);
}

// 2. Hash 命令回调：HMSET
static void redis_hash_hmset_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r || r->type == REDIS_REPLY_ERROR) {
		printf("HMSET %s failed: %s\n", key, r ? r->str : "unknown error");
		return;
	}
	printf("HMSET %s success: %s\n", key, r->str);
}

// Hash 命令回调：HGETALL
static void redis_hash_hgetall_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r) {
		printf("HGETALL %s failed: unknown error\n", key);
		return;
	}
	if (r->type == REDIS_REPLY_ERROR) {
		printf("HGETALL %s failed: %s\n", key, r->str);
		return;
	}
	if (r->type != REDIS_REPLY_ARRAY) {
		printf("HGETALL %s: invalid reply type\n", key);
		return;
	}
	printf("HGETALL %s success (total %lu elements):\n", key, r->elements);
	for (size_t i = 0; i < r->elements; i += 2) {
		printf("  %s: %s\n", r->element[i]->str, r->element[i + 1]->str);
	}
}

// 3. List 命令回调：LPUSH
static void redis_list_lpush_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r || r->type != REDIS_REPLY_INTEGER) {
		printf("LPUSH %s failed: %s\n", key, r ? r->str : "unknown error");
		return;
	}
	printf("LPUSH %s success: list length = %lld\n", key, r->integer);
}

// List 命令回调：LRANGE
static void redis_list_lrange_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r) {
		printf("LRANGE %s failed: unknown error\n", key);
		return;
	}
	if (r->type == REDIS_REPLY_ERROR) {
		printf("LRANGE %s failed: %s\n", key, r->str);
		return;
	}
	if (r->type != REDIS_REPLY_ARRAY) {
		printf("LRANGE %s: invalid reply type\n", key);
		return;
	}
	printf("LRANGE %s success (total %lu elements):\n", key, r->elements);
	for (size_t i = 0; i < r->elements; i++) {
		printf("  %lu: %s\n", i, r->element[i]->str);
	}
}

// 4. Set 命令回调：SADD
static void redis_set_sadd_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r || r->type != REDIS_REPLY_INTEGER) {
		printf("SADD %s failed: %s\n", key, r ? r->str : "unknown error");
		return;
	}
	printf("SADD %s success: added %lld elements\n", key, r->integer);
}

// Set 命令回调：SMEMBERS
static void redis_set_smembers_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r) {
		printf("SMEMBERS %s failed: unknown error\n", key);
		return;
	}
	if (r->type == REDIS_REPLY_ERROR) {
		printf("SMEMBERS %s failed: %s\n", key, r->str);
		return;
	}
	if (r->type != REDIS_REPLY_ARRAY) {
		printf("SMEMBERS %s: invalid reply type\n", key);
		return;
	}
	printf("SMEMBERS %s success (total %lu elements):\n", key, r->elements);
	for (size_t i = 0; i < r->elements; i++) {
		printf("  %lu: %s\n", i, r->element[i]->str);
	}
}

// 5. ZSet 命令回调：ZADD
static void redis_zset_zadd_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r || r->type != REDIS_REPLY_INTEGER) {
		printf("ZADD %s failed: %s\n", key, r ? r->str : "unknown error");
		return;
	}
	printf("ZADD %s success: added %lld elements\n", key, r->integer);
}

// ZSet 命令回调：ZRANGE
static void redis_zset_zrange_cb(redisAsyncContext* ac, void* reply, void* privdata) {
	redisReply* r = (redisReply*)reply;
	const char* key = (const char*)privdata;
	if (!r) {
		printf("ZRANGE %s failed: unknown error\n", key);
		return;
	}
	if (r->type == REDIS_REPLY_ERROR) {
		printf("ZRANGE %s failed: %s\n", key, r->str);
		return;
	}
	if (r->type != REDIS_REPLY_ARRAY) {
		printf("ZRANGE %s: invalid reply type\n", key);
		return;
	}
	printf("ZRANGE %s success (total %lu elements, member:score):\n", key, r->elements);
	for (size_t i = 0; i < r->elements; i += 2) {
		printf("  %s: %s\n", r->element[i]->str, r->element[i + 1]->str);
	}
}

int main()
{
	g_reactor = create_reactor();
	if (!g_reactor) {
		printf("Failed to create reactor\n");
		return -1;
	}

	signal(SIGINT, sigint_handler);

	event_t* redis_event = reactor_redis_async_connect(g_reactor, "127.0.0.1", 6379);
	if (!redis_event) {
		release_reactor(g_reactor);
		g_reactor = NULL;
		return -1;
	}

	// 4. 发送 Redis 异步命令（覆盖所有数据结构，绑定回调）
	// 4.1 String 命令
	reactor_redis_async_send_cmd(redis_event, redis_string_set_cb, "str:name", "SET str:name 'async-redis'");
	reactor_redis_async_send_cmd(redis_event, redis_string_get_cb, "str:name", "GET str:name");
	reactor_redis_async_send_cmd(redis_event, redis_string_incr_cb, "str:counter", "INCR str:counter");

	// 4.2 Hash 命令
	reactor_redis_async_send_cmd(redis_event, redis_hash_hmset_cb, "hash:user", "HMSET hash:user id 200 name 'async-tom' age 22");
	reactor_redis_async_send_cmd(redis_event, redis_hash_hgetall_cb, "hash:user", "HGETALL hash:user");

	// 4.3 List 命令
	reactor_redis_async_send_cmd(redis_event, redis_list_lpush_cb, "list:fruits", "LPUSH list:fruits 'async-apple' 'async-banana'");
	reactor_redis_async_send_cmd(redis_event, redis_list_lrange_cb, "list:fruits", "LRANGE list:fruits 0 -1");

	// 4.4 Set 命令
	reactor_redis_async_send_cmd(redis_event, redis_set_sadd_cb, "set:tags", "SADD set:tags 'async-c' 'async-c++'");
	reactor_redis_async_send_cmd(redis_event, redis_set_smembers_cb, "set:tags", "SMEMBERS set:tags");

	// 4.5 ZSet 命令
	reactor_redis_async_send_cmd(redis_event, redis_zset_zadd_cb, "zset:ranks", "ZADD zset:ranks 92 'async-alice' 88 'async-bob'");
	reactor_redis_async_send_cmd(redis_event, redis_zset_zrange_cb, "zset:ranks", "ZRANGE zset:ranks 0 -1 WITHSCORES");

	eventloop(g_reactor);

	redisAsyncContext* ac = (redisAsyncContext*)redis_event->priv;
	redisAsyncDisconnect(ac);
	del_event(g_reactor, redis_event); // 删除 Reactor 事件
	release_reactor(g_reactor);        // 释放 Reactor

	printf("All resources released\n");
	return 0;
}