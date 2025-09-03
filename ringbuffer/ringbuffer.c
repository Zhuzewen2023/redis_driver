#include "ringbuffer.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>

static inline bool is_power_of_two(uint32_t num)
{
	return (num != 0) && ((num & (num - 1)) == 0);
}

static inline uint32_t roundup_power_of_two(uint32_t num)
{
	if (num == 0) return 1;
	uint32_t result = 1;
	while (result < num) {
		result <<= 1;
		if (result == 0) return 1 << 31;
	}
	return result;
}

ringbuffer_t* ringbuffer_create(uint32_t sz)
{
	uint32_t size = roundup_power_of_two(sz);
	if (size == 0) return NULL;
	ringbuffer_t* rb = (ringbuffer_t*)malloc(sizeof(ringbuffer_t) + size);
	if (!rb) return NULL;

	rb->size = size;
	atomic_init(&rb->head, 0);
	atomic_init(&rb->tail, 0);
	rb->buf = (uint8_t*)(rb + 1);

	return rb;
}

void ringbuffer_destroy(ringbuffer_t* rb)
{
	if (rb) {
		free(rb);
	}
}

size_t ringbuffer_used(ringbuffer_t* rb)
{
	assert(rb != NULL);
	uint32_t head = atomic_load(&rb->head);
	uint32_t tail = atomic_load(&rb->tail);
	return tail - head;
}

size_t ringbuffer_available(ringbuffer_t* rb)
{
	assert(rb != NULL);
	return rb->size - ringbuffer_used(rb);
}

size_t ringbuffer_write(ringbuffer_t* rb, const void* data, size_t len)
{
	if (!rb || !data || len == 0) return 0;

	const uint8_t* src = (const uint8_t*)data;
	uint32_t head = atomic_load(&rb->head);
	uint32_t tail = atomic_load(&rb->tail);
	size_t available = rb->size - (tail - head);
	if (len > available) {
		len = available;
		if (len == 0) return 0;
	}

	uint32_t pos = tail & (rb->size - 1);
	size_t to_end = rb->size - pos;

	if (len <= to_end) {
		memcpy(rb->buf + pos, src, len);
	}
	else {
		memcpy(rb->buf + pos, src, to_end);
		memcpy(rb->buf, src + to_end, len - to_end);
	}

	atomic_store(&rb->tail, tail + len);
	return len;
}

size_t ringbuffer_read(ringbuffer_t* rb, void* data, size_t len)
{
	if (!rb || !data || len == 0) return 0;
	
	uint8_t* dst = (uint8_t*)data;
	uint32_t head = atomic_load(&rb->head);
	uint32_t tail = atomic_load(&rb->tail);
	size_t used = tail - head;

	if (len > used) {
		len = used;
		if (len == 0) return 0;
	}

	uint32_t pos = head & (rb->size - 1); //index % size
	size_t to_end = rb->size - pos;

	if (len <= to_end) {
		memcpy(dst, rb->buf + pos, len);
	}
	else {
		memcpy(dst, rb->buf + pos, to_end);
		memcpy(dst + to_end, rb->buf, len - to_end);
	}

	atomic_store(&rb->head, head + len);
	return len;
}

void ringbuffer_clear(ringbuffer_t* r)
{
	if (!r) return;
	uint32_t tail = atomic_load(&r->tail);
	atomic_store(&r->head, tail);
}

size_t ringbuffer_find(ringbuffer_t* rb, const char* sep, size_t seplen)
{
	if (!rb || !sep || seplen == 0) return 0;

	size_t used = ringbuffer_used(rb);
	if (used < seplen) return 0;

	uint32_t head = atomic_load(&rb->head);
	uint32_t head_pos = head & (rb->size - 1);

	for (size_t i = 0; i <= used - seplen; i++) {
		uint32_t pos = (head_pos + i) & (rb->size - 1);
		if (pos + seplen <= rb->size) {
			if (memcmp(rb->buf + pos, sep, seplen) == 0) {
				return i + seplen;
			}
		}
		else {
			size_t first_part = rb->size - pos;
			if (memcmp(rb->buf + pos, sep, first_part) != 0) {
				continue;
			}
			if (memcmp(rb->buf, sep + first_part, seplen - first_part) == 0) {
				return i + seplen;
			}
		}
	}
	return 0;
}

size_t ringbuffer_get_contiguous(ringbuffer_t* rb, uint8_t** data_ptr)
{
	if (!rb || !data_ptr) return 0;

	uint32_t head = atomic_load(&rb->head);
	uint32_t tail = atomic_load(&rb->tail);
	uint32_t head_pos = head & (rb->size - 1);
	uint32_t tail_pos = tail & (rb->size - 1);

	size_t contiguous_len;
	if (tail >= head) {
		// 数据未跨边界
		contiguous_len = tail_pos - head_pos;
	}
	else {
		contiguous_len = rb->size - head_pos;
	}

	*data_ptr = rb->buf + head_pos;
	return contiguous_len;
}