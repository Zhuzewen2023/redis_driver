#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct ringbuffer_s ringbuffer_t;

struct ringbuffer_s
{
	uint32_t size;
	atomic_uint_fast32_t head;
	atomic_uint_fast32_t tail;
	uint8_t* buf;
};

ringbuffer_t* ringbuffer_create(uint32_t size);

void ringbuffer_destroy(ringbuffer_t* rb);

size_t ringbuffer_write(ringbuffer_t* rb, const void* data, size_t len);

size_t ringbuffer_read(ringbuffer_t* rb, void* data, size_t len);

void ringbuffer_clear(ringbuffer_t* rb);

size_t ringbuffer_used(ringbuffer_t* rb);

size_t ringbuffer_available(ringbuffer_t* rb);

size_t ringbuffer_find(ringbuffer_t* rb, const char* sep, size_t seplen);

size_t ringbuffer_get_contiguous(ringbuffer_t* rb, uint8_t** data_ptr);

#endif