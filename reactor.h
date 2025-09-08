#ifndef __Z2W_REACTOR_H__
#define __Z2W_REACTOR_H__

#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h> //read write
#include <fcntl.h> //fcntl
#include <sys/types.h> //listen
#include <sys/socket.h> // socket
#include <errno.h> //errno
#include <arpa/inet.h> //inet_addr htons
#include <assert.h> //assert
#include <stdlib.h> //malloc
#include <string.h> //memcpy memmove

#include "chainbuffer/chainbuffer.h"

#define MAX_EVENT_NUM	1024
#define MAX_CONN ((1 << 16) - 1) //16位无符号整数能表示的最大值

typedef struct event_s event_t;
typedef struct reactor_s reactor_t;

typedef void (*event_callback_fn)(int fd, int events, void* privdata);
typedef void (*error_callback_fn)(int fd, char* err);

struct event_s
{
	int fd;
	reactor_t* r;
	buffer_t* in;
	buffer_t* out;
	event_callback_fn read_fn;
	event_callback_fn write_fn;
	error_callback_fn error_fn;
	void* priv;
};

struct reactor_s
{
	int epfd;
	int listenfd;
	int stop;
	event_t* events;
	int iter;
	struct epoll_event fire[MAX_EVENT_NUM];
};

int event_buffer_read(event_t* e);
int event_buffer_write(event_t* e, void* buf, int sz);

reactor_t* create_reactor(void);
void release_reactor(reactor_t* r);
event_t* new_event(reactor_t* r, int fd, event_callback_fn rd, event_callback_fn wt, error_callback_fn err);
void free_event(event_t* e);

buffer_t* evbuf_in(event_t* e);

buffer_t* evbuf_out(event_t* e);

reactor_t* event_base(event_t* e);

int set_nonblock(int fd);

int add_event(reactor_t* R, int events, event_t* e);

int del_event(reactor_t* R, event_t* e);

int enable_event(reactor_t* R, event_t* e, int readable, int writeable);

void eventloop_once(reactor_t* r, int timeout);

void stop_eventloop(reactor_t* r);

void eventloop(reactor_t* r);

int create_server(reactor_t* R, short port, event_callback_fn func);

int event_buffer_read(event_t* e);

int event_buffer_write(event_t* e, void* buf, int sz);

#endif
