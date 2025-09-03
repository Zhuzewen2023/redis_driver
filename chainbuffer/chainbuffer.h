#ifndef __CHAIN_BUFFER_H__
#define __CHAIN_BUFFER_H__

#include <stdint.h>

typedef struct buf_chain_s buf_chain_t;
typedef struct buffer_s buffer_t;

struct buf_chain_s
{
	struct buf_chain_s* next;
	uint32_t buffer_len;
	uint32_t misalign;
	uint32_t off;
	uint8_t* buffer;
};

struct buffer_s
{
	buf_chain_t* first;
	buf_chain_t* last;
	buf_chain_t** last_with_datap; //假设有块A-->B,A的next指针指向B，这个二级指针实际上存储了块A的next指针的地址，便于直接修改块A的next指针
	uint32_t total_len;
	uint32_t last_read_pos; //for sep read
};

#define CHAIN_SPACE_LEN(ch) ((ch)->buffer_len - ((ch)->misalign + (ch)->off))
#define MIN_BUFFER_SIZE				1024	//单个数据块的 最小内存大小（字节）。
#define MAX_TO_COPY_IN_EXPAND		4096	//扩容时 单次最多拷贝的字节数（虽代码中未显式调用，但设计意图是：合并数据块时，避免单次拷贝太多导致单线程阻塞）。
#define BUFFER_CHAIN_MAX_AUTO_SIZE	4096	//数据块 自动扩容的最大阈值。创建数据块时，若需要的尺寸≤4096，按 “2 的幂次” 自动扩容（比如要 500 字节，扩到 1024）；超过则按实际尺寸分配（避免扩太大浪费）。
#define MAX_TO_REALIGN_IN_EXPAND	2048	//内存对齐的 最大偏移量阈值。在 buf_chain_should_realign 中判断：只有当已用数据长度（off）≤2048 时，才会做内存对齐（buf_chain_align）—— 超过则对齐成本太高（memmove 拷贝数据太多），放弃对齐。
#define BUFFER_CHAIN_MAX			16 * 1024 * 1024 //16M
#define BUFFER_CHAIN_EXTRA(t, c)	(t*)((buf_chain_t*)(c) + 1) //计算数据块中 实际数据区的起始地址。设计逻辑：buf_chain_s 结构体和数据区分配在同一块内存（结构体在前，数据区在后），(buf_chain_t*)(c) + 1 表示 “跳过整个结构体的大小”（指针加 1 按结构体大小偏移），再转成目标类型 t * （比如 uint8_t * ），就是数据区的起始地址。
#define BUFFER_CHAIN_SIZE			(sizeof(buf_chain_t))

buffer_t* buffer_new(uint32_t sz);

uint32_t buffer_len(buffer_t* buf);

int buffer_add(buffer_t* buf, const void* data, uint32_t datlen);

int buffer_remove(buffer_t* buf, void* data, uint32_t datlen);

int buffer_drain(buffer_t* buf, uint32_t len);

void buffer_free(buffer_t* buf);

int buffer_search(buffer_t* buf, const char* sep, const int seplen);

uint8_t* buffer_write_atmost(buffer_t* p);

#endif
