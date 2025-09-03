#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "chainbuffer.h"  // 引入你的缓冲区头文件

// -------------------------- 辅助工具函数 --------------------------
// 打印测试结果
#define TEST_START(name) printf("\n[TEST] %s ... ", name)
#define TEST_PASS() printf("PASS\n")
#define TEST_FAIL() printf("FAIL\n")

// 模拟内存分配失败（用于测试异常场景，可选）
#ifdef SIMULATE_MALLOC_FAIL
static int malloc_fail_flag = 0;
void* __real_malloc(size_t size);
void* __wrap_malloc(size_t size) {
    if (malloc_fail_flag && size > 0) {
        malloc_fail_flag = 0;
        return NULL;
    }
    return __real_malloc(size);
}
void simulate_malloc_fail() { malloc_fail_flag = 1; }
#else
#define simulate_malloc_fail() (void)0  // 空实现，不模拟失败
#endif


// -------------------------- 测试用例 --------------------------
// 测试1：缓冲区创建与释放（基础功能）
void test_buffer_create_free() {
    TEST_START("buffer_create_free");
    buffer_t* buf = buffer_new(0);
    assert(buf != NULL);          // 创建成功
    assert(buffer_len(buf) == 0); // 初始长度为0
    assert(buf->first == NULL);   // 初始无数据块
    buffer_free(buf);             // 释放无崩溃（防内存泄漏，可配合valgrind验证）
    TEST_PASS();
}

// 测试2：单块数据添加与读取
void test_buffer_single_block_add_remove() {
    TEST_START("single_block_add_remove");
    buffer_t* buf = buffer_new(0);
    assert(buf != NULL);

    // 1. 添加数据（单块可容纳）
    const char* data = "hello chainbuffer!";
    int data_len = strlen(data);
    int ret = buffer_add(buf, data, data_len);
    assert(ret == 0);                  // 添加成功
    assert(buffer_len(buf) == data_len);// 长度正确

    // 2. 读取数据
    char read_buf[1024] = { 0 };
    int read_len = buffer_remove(buf, read_buf, data_len);
    assert(read_len == data_len);      // 读取长度正确
    assert(strcmp(read_buf, data) == 0);// 数据内容一致

    // 3. 读取后缓冲区为空
    assert(buffer_len(buf) == 0);

    buffer_free(buf);
    TEST_PASS();
}

// 测试3：跨块数据添加与合并（buffer_write_atmost）
void test_buffer_cross_block_merge() {
    TEST_START("cross_block_merge");
    buffer_t* buf = buffer_new(0);
    assert(buf != NULL);

    // 1. 添加跨块数据（触发新建块：MIN_BUFFER_SIZE=1024，分两次添加1500字节）
    char data1[1200] = { 0 };
    char data2[800] = { 0 };
    memset(data1, 'A', sizeof(data1));
    memset(data2, 'B', sizeof(data2));
    int len1 = sizeof(data1), len2 = sizeof(data2);

    assert(buffer_add(buf, data1, len1) == 0);
    assert(buffer_add(buf, data2, len2) == 0);
    assert(buffer_len(buf) == len1 + len2); // 总长度正确

    // 2. 合并数据（buffer_write_atmost）
    uint8_t* merged_buf = buffer_write_atmost(buf);
    assert(merged_buf != NULL);

    // 3. 验证合并后数据（前1200字节为'A'，后800字节为'B'）
    bool ok = true;
    for (int i = 0; i < len1; i++) {
        if (merged_buf[i] != 'A') { ok = false; break; }
    }
    for (int i = 0; i < len2; i++) {
        if (merged_buf[len1 + i] != 'B') { ok = false; break; }
    }
    assert(ok);

    // 4. 合并后最多2块（验证间接：读取合并后数据完整）
    char read_buf[2048] = { 0 };
    assert(buffer_remove(buf, read_buf, len1 + len2) == len1 + len2);
    assert(strncmp(read_buf, data1, len1) == 0);
    assert(strncmp(read_buf + len1, data2, len2) == 0);

    buffer_free(buf);
    TEST_PASS();
}

// 测试4：数据删除（buffer_drain）
void test_buffer_drain() {
    TEST_START("buffer_drain");
    buffer_t* buf = buffer_new(0);
    assert(buf != NULL);

    // 1. 添加3段数据（模拟多块）
    const char* data = "abcdefghijklmnopqrst";
    int data_len = strlen(data);
    assert(buffer_add(buf, data, data_len) == 0);
    assert(buffer_len(buf) == data_len);

    // 2. 部分删除（删除前5字节）
    int drain_len1 = 5;
    assert(buffer_drain(buf, drain_len1) == drain_len1);
    assert(buffer_len(buf) == data_len - drain_len1);

    // 验证剩余数据（从第6字节开始）
    char read_buf1[100] = { 0 };
    assert(buffer_remove(buf, read_buf1, data_len - drain_len1) == data_len - drain_len1);
    assert(strcmp(read_buf1, data + drain_len1) == 0);

    // 3. 全量删除（空缓冲区）
    assert(buffer_add(buf, data, data_len) == 0);
    assert(buffer_drain(buf, data_len + 10) == data_len); // 超长度删除→删全部
    assert(buffer_len(buf) == 0);
    assert(buffer_remove(buf, read_buf1, 10) == 0); // 空缓冲区读取返回0

    buffer_free(buf);
    TEST_PASS();
}

// 测试5：分隔符搜索（单块/跨块匹配）
void test_buffer_search() {
    TEST_START("buffer_search");
    buffer_t* buf = buffer_new(0);
    assert(buf != NULL);

    // 场景1：单块内找到分隔符
    const char* data1 = "hello\nworld";
    const char* sep1 = "\n";
    int seplen1 = strlen(sep1);
    assert(buffer_add(buf, data1, strlen(data1)) == 0);
    int pos1 = buffer_search(buf, sep1, seplen1);
    assert(pos1 == 6); // 分隔符末尾在第6字节（0-based：hello(5) + \n(1) → 5+1=6）

    // 场景2：跨块找到分隔符
    buffer_drain(buf, buffer_len(buf)); // 清空缓冲区
    const char* data2_part1 = "abcdef";
    const char* data2_part2 = "gh\r\nijkl";
    const char* sep2 = "\r\n";
    int seplen2 = strlen(sep2);
    assert(buffer_add(buf, data2_part1, strlen(data2_part1)) == 0);
    assert(buffer_add(buf, data2_part2, strlen(data2_part2)) == 0);
    int pos2 = buffer_search(buf, sep2, seplen2);
    assert(pos2 == 6 + 2 + 2); // 6(part1) + 2(gh) + 2(\r\n) = 10

    // 场景3：未找到分隔符（验证last_read_pos）
    buffer_drain(buf, buffer_len(buf));
    const char* data3 = "1234567890";
    const char* sep3 = "xyz";
    int seplen3 = strlen(sep3);
    assert(buffer_add(buf, data3, strlen(data3)) == 0);
    int pos3 = buffer_search(buf, sep3, seplen3);
    assert(pos3 == 0); // 未找到返回0
    assert(buf->last_read_pos == strlen(data3) - seplen3 + 1); // 搜索到末尾

    buffer_free(buf);
    TEST_PASS();
}

// 测试6：异常场景（数据超上限、内存分配失败）
void test_buffer_exception() {
    TEST_START("buffer_exception");
    buffer_t* buf = buffer_new(0);
    assert(buf != NULL);

    // 场景1：添加数据超最大容量（BUFFER_CHAIN_MAX=16*1024*1024）
    uint32_t max_size = 16 * 1024 * 1024;
    uint32_t over_size = max_size + 100;
    char* big_data = (char*)malloc(over_size);
    assert(big_data != NULL);
    int ret = buffer_add(buf, big_data, over_size);
    assert(ret == -1); // 超上限添加失败
    free(big_data);

    // 场景2：内存分配失败（需开启SIMULATE_MALLOC_FAIL编译选项）
#ifdef SIMULATE_MALLOC_FAIL
    simulate_malloc_fail();
    buffer_t* fail_buf = buffer_new(0);
    assert(fail_buf == NULL); // malloc失败→创建缓冲区失败

    simulate_malloc_fail();
    buffer_t* ok_buf = buffer_new(0);
    assert(ok_buf != NULL);
    ret = buffer_add(ok_buf, "test", 4); // 第二次malloc失败→添加失败
    assert(ret == -1);
    buffer_free(ok_buf);
#endif

    buffer_free(buf);
    TEST_PASS();
}


// -------------------------- 主函数（执行所有测试） --------------------------
int main() {
    printf("=== ChainBuffer Test Start ===\n");

    test_buffer_create_free();
    test_buffer_single_block_add_remove();
    test_buffer_cross_block_merge();
    test_buffer_drain();
    test_buffer_search();
    test_buffer_exception();

    printf("\n=== All Tests Finished ===\n");
    return 0;
}