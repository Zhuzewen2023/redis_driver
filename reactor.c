#include "reactor.h"

reactor_t* create_reactor()
{
	reactor_t* r = (reactor_t*)malloc(sizeof(reactor_t));
	r->epfd = epoll_create(1);
	r->listenfd = -1;
	r->stop = 0;
	r->iter = 0;
	r->events = (event_t*)malloc(sizeof(event_t) * MAX_CONN);
	memset(r->events, 0, sizeof(event_t) * MAX_CONN);
	memset(r->fire, 0, sizeof(struct epoll_event) * MAX_EVENT_NUM);
	return r;
}

void release_reactor(reactor_t* r)
{
	free(r->events);
	close(r->epfd);
	free(r);
}

static event_t* _get_event_t(reactor_t* r)
{
	r->iter++;
	while (r->events[r->iter & MAX_CONN].fd > 0) { 
		r->iter++;
	}
	return &r->events[r->iter];
}

event_t* new_event(reactor_t* r, int fd, event_callback_fn rd, event_callback_fn wt, event_callback_fn err)
{
	assert(rd || wt || err);
	event_t* e = _get_event_t(r);
	e->r = r;
	e->fd = fd;
	e->read_fn = rd;
	e->write_fn = wt;
	e->error_fn = err;
	return e;
}

void free_event(event_t* e)
{
	
}

int set_nonblock(int fd)
{
	int flag = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int add_event(reactor_t* r, int events, event_t* e)
{
	struct epoll_event ev;
	ev.events = events;
	ev.data.ptr = e;
	if (epoll_ctl(r->epfd, EPOLL_CTL_ADD, e->fd, &ev) == -1) {
		printf("add event err fd = %d\n", e->fd);
		return -1;
	}
	return 0;
}

int del_event(reactor_t* r, event_t* e)
{
	epoll_crl(r->epfd, EPOLL_CTL_DEL, e->fd, NULL);
	free_event(e);
	return 0;
}

int enable_event(reactor_t* r, event_t* e, int readable, int writeable)
{
	struct epoll_event ev;
	ev.events = (readable ? EPOLLIN : 0) | (writable ? EPOLLOUT : 0);
	ev.data.ptr = e;
	if (epoll_ctl(r->epfd, EPOLL_CTL_MOD, e->fd, &ev) == -1) {
		return -1;
	}
	return 0;
}

void eventloop_once(reactor_t* r, int timeout)
{
	int n = epoll_wait(r->epfd, r->fire, MAX_EVENT_NUM, timeout);
	for (int i = 0; i < n; i++) {
		struct epoll_event* e = &r->fire[i];
		int mask = e->events;
		if (e->events & EPOLLERR) mask |= EPOLLIN | EPOLLOUT;
		if (e->events & EPOLLHUP) mask |= EPOLLIN | EPOLLOUT;
		event_t* et = (event_t*)e->data.ptr;
		if (mask & EPOLLIN) {
			if (et->read_fn) {
				et->read_fn(et->fd, EPOLLIN, et);
			}
		}
		if (mask & EPOLLOUT) {
			if (er->write_fn) {
				et->write_fn(et->fd, EPOLLOUT, et);
			}
		}
	}
}

void stop_eventloop(reactor_t* r)
{
	r->stop = 1;
}

void eventloop(reactor_t* r)
{
	while (!r->stop) {
		eventloop_once(r, -1);
	}
}

int create_server(reactor_t* r, short port, event_callback_fn func)
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0) {
		printf("create listen fd error\n");
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = INADDR_ANY;

	int reuse = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(int)) == -1) {
		printf("reuse address error: %s\n", strerror(errno));
		return -1;
	}

	if (bind(listenfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0) {
		printf("bind error %s\n", strerror(errno));
		return -2;
	}

	if (listen(listenfd, 5) < 0) {
		printf("listen error %s\n", strerror(errno));
		return -3;
	}

	if (set_nonblock(listenfd) < 0) {
		printf("set_nonblock error %s\n", strerror(errno));
		return -4;
	}

	r->listenfd = listenfd;

	event_t* e = new_event(r, listenfd, func, NULL, NULL);
	add_event(r, EPOLLIN, e);

	printf("listen port : %d\n", port);
	return 0;
}

int event_buffer_read(event_t* e)
{
	int fd = e->fd;
	int num = 0;
	while (1) {
		char buf[1024] = { 0 };
		int n = read(fd, buf, 1024);
		if (n == 0) {
			printf("close connection fd = %d\n", fd);
			if (e->error_fn) {
				e->error_fn(fd, "close socket");
			}
			del_event(e->r, e);
			close(fd);
			return 0;
		}
		else if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EWOULDBLOCK) {
				break;
			}
			printf("read error fd = %d err = %s\n", fd, strerror(errno));
			if (e->error_fn) {
				e->error_fn(fd, strerr(errno));
			}
			del_event(e->r, e);
			close(fd);
			return 0;
		}
		else {
			printf("recv data from client:%s", buf);
			//buffer_add(evbuf_in(e), buf, n);
		}
		num += n;
	}
	return num;
}

static int _write_socket(event_t* e, void* buf, int size)
{
	int fd = e->fd;
	while (1) {
		int n = write(fd, buf, size);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (errno == EWOULDBLOCK) {
				break;
			}
			if (e->error_fn) {
				e->error_fn(fd, strerr(errno));
			}
			del_event(e->r, e);
			close(e->fd);
		}
		return n;
	}
	return 0;
}

int event_buffer_write(event_t* e, void* buf, int sz) {
	//buffer_t* out = evbuf_out(e);
	//if (buffer_len(buf) == 0) {
		//int n = _write_socket(e, buf, sz);
		//if (n == 0 || n < sz) {
			// 发送失败，除了将没有发送出去的数据写入缓冲区，还要注册写事件
			//buffer_add(out, (char*)buf + n, sz - n);
			//enable_event(e->r, e, 1, 1);
			//return 0;
		//}
		//else if (n < 0)
			//return 0;
		//return 1;
	//}
	//buffer_add(out, (char*)buf, sz);
	return 1;
}