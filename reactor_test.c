#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "reactor.h"  // 包含你的 Reactor 头文件

void accept_cb(int listen_fd, int events, void* arg);
void client_read_cb(int client_fd, int events, void* arg);
void client_error_cb(int client_fd, char* err_msg);

// -------------------------- 回调函数实现 --------------------------
// 1. 监听 fd 的 ACCEPT 回调（处理新客户端连接）
void accept_cb(int listen_fd, int events, void* arg) {
    event_t* et = (event_t*)arg;
    reactor_t* r = et->r;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // 非阻塞 accept（防止无连接时阻塞）
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (errno == EINTR || errno == EWOULDBLOCK) {
            return;  // 正常重试场景，忽略错误
        }
        printf("accept error: %s\n", strerror(errno));
        return;
    }

    printf("new client connected: fd=%d, ip=%s, port=%d\n",
        client_fd,
        inet_ntoa(client_addr.sin_addr),
        ntohs(client_addr.sin_port));

    // 设置客户端 fd 为非阻塞（Reactor 需非阻塞 IO）
    if (set_nonblock(client_fd) < 0) {
        close(client_fd);
        printf("set client fd nonblock error\n");
        return;
    }

    // 创建客户端事件：绑定读回调（无写回调/错误回调）
    event_t* client_et = new_event(r, client_fd,
        client_read_cb,  // 客户端读事件回调
        NULL,            // 无写回调（用默认写逻辑）
        client_error_cb  // 客户端错误回调
    );
    if (client_et == NULL) {
        close(client_fd);
        printf("create client event error\n");
        return;
    }

    // 给客户端 fd 添加 EPOLLIN 事件（监听读事件）
    if (add_event(r, EPOLLIN, client_et) < 0) {
        free_event(client_et);
        close(client_fd);
        printf("add client event error\n");
        return;
    }
}

// 2. 客户端 fd 的 READ 回调（接收数据并回声）
void client_read_cb(int client_fd, int events, void* arg) {
    event_t* et = (event_t*)arg;
    buffer_t* in_buf = evbuf_in(et);

    // 1. 读取客户端数据到输入缓冲区
    int read_len = event_buffer_read(et);
    if (read_len <= 0) {
        // event_buffer_read 内部已处理关闭连接，无需额外操作
        printf("client fd=%d read done/error, connection closed\n", client_fd);
        return;
    }

    // 2. 回声逻辑：将输入缓冲区的数据写入输出缓冲区，触发发送
    char* data = buffer_write_atmost(in_buf);  // 假设 buffer_data 返回缓冲区数据指针
    int data_len = buffer_len(in_buf);

    printf("recv from client fd=%d: %.*s", client_fd, data_len, data);  // 打印接收数据

    // 3. 调用 Reactor 发送接口，将数据发回客户端
    int ret = event_buffer_write(et, data, data_len);
    if (ret != 1) {
        printf("client fd=%d send pending, wait EPOLLOUT\n", client_fd);
    }
    else {
        printf("send to client fd=%d: %.*s", client_fd, data_len, data);  // 打印发送数据
    }

    // 4. 清空输入缓冲区（准备下次接收）
    buffer_drain(in_buf, data_len);
}

// 3. 客户端 fd 的 ERROR 回调（连接异常时处理）
void client_error_cb(int client_fd, char* err_msg) {
    printf("client fd=%d error: %s, closing connection\n", client_fd, err_msg);
    // 错误处理已在 event_buffer_read/_write_socket 中完成（del_event + close）
}

// -------------------------- 主函数（启动服务器） --------------------------
int main() {
    // 1. 创建 Reactor 实例
    reactor_t* r = create_reactor();
    if (r == NULL) {
        printf("create reactor error\n");
        return -1;
    }

    // 2. 创建 TCP 服务器（监听 8888 端口，连接回调为 accept_cb）
    int ret = create_server(r, htons(8888), accept_cb);
    if (ret != 0) {
        printf("create server error, code=%d\n", ret);
        release_reactor(r);
        return -1;
    }

    // 3. 启动 Reactor 事件循环（阻塞，直到 stop_eventloop 被调用）
    printf("reactor event loop start, listening port 8888...\n");
    eventloop(r);

    // 4. 释放 Reactor 资源（实际需信号触发 stop_eventloop 才会执行到这）
    release_reactor(r);
    return 0;
}