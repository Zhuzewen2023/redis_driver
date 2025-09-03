#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ringbuffer.h"

// 测试用例1：基本写入和读取
void test_basic_write_read() {
    printf("Test 1: Basic write and read\n");
    ringbuffer_t* rb = ringbuffer_create(8);
    assert(rb != NULL);
    assert(ringbuffer_used(rb) == 0);
    assert(ringbuffer_available(rb) == 8);

    // 写入数据
    const char* test_data = "Hello";
    size_t written = ringbuffer_write(rb, test_data, strlen(test_data));
    assert(written == 5);
    assert(ringbuffer_used(rb) == 5);
    assert(ringbuffer_available(rb) == 3);

    // 读取数据
    char buffer[10];
    size_t read = ringbuffer_read(rb, buffer, sizeof(buffer));
    assert(read == 5);
    buffer[read] = '\0';
    assert(strcmp(buffer, "Hello") == 0);
    assert(ringbuffer_used(rb) == 0);
    assert(ringbuffer_available(rb) == 8);

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例2：环形写入（跨越缓冲区末尾）
void test_wrap_around() {
    printf("Test 2: Wrap-around writing\n");
    ringbuffer_t* rb = ringbuffer_create(8);
    assert(rb != NULL);

    // 写入6字节，使尾部接近边界
    const char* data1 = "ABCDEF";
    size_t written = ringbuffer_write(rb, data1, 6);
    assert(written == 6);

    // 读取4字节，释放空间
    char buffer[10];
    size_t read = ringbuffer_read(rb, buffer, 4);
    assert(read == 4);
    buffer[read] = '\0';
    assert(strcmp(buffer, "ABCD") == 0);

    // 写入4字节，应该会跨越边界
    const char* data2 = "GHIJ";
    written = ringbuffer_write(rb, data2, 4);
    assert(written == 4);

    // 检查缓冲区内容
    assert(ringbuffer_used(rb) == 6);
    read = ringbuffer_read(rb, buffer, 6);
    assert(read == 6);
    buffer[read] = '\0';
    assert(strcmp(buffer, "EFGHIJ") == 0);

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例3：缓冲区满时写入
void test_full_write() {
    printf("Test 3: Writing when full\n");
    ringbuffer_t* rb = ringbuffer_create(8);
    assert(rb != NULL);

    // 填满缓冲区
    const char* data = "12345678";
    size_t written = ringbuffer_write(rb, data, 8);
    assert(written == 8);
    assert(ringbuffer_used(rb) == 8);
    assert(ringbuffer_available(rb) == 0);

    // 尝试写入更多数据
    written = ringbuffer_write(rb, "9", 1);
    assert(written == 0); // 应该失败
    assert(ringbuffer_used(rb) == 8);

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例4：缓冲区空时读取
void test_empty_read() {
    printf("Test 4: Reading when empty\n");
    ringbuffer_t* rb = ringbuffer_create(8);
    assert(rb != NULL);

    char buffer[10];
    size_t read = ringbuffer_read(rb, buffer, sizeof(buffer));
    assert(read == 0); // 应该失败

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例5：清空缓冲区
void test_clear() {
    printf("Test 5: Clearing buffer\n");
    ringbuffer_t* rb = ringbuffer_create(8);
    assert(rb != NULL);

    // 写入一些数据
    ringbuffer_write(rb, "Test", 4);
    assert(ringbuffer_used(rb) == 4);

    // 清空缓冲区
    ringbuffer_clear(rb);
    assert(ringbuffer_used(rb) == 0);
    assert(ringbuffer_available(rb) == 8);

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例6：查找模式
void test_find_pattern() {
    printf("Test 6: Finding pattern\n");
    ringbuffer_t* rb = ringbuffer_create(16);
    assert(rb != NULL);

    // 写入数据
    const char* data = "HelloWorld123";
    ringbuffer_write(rb, data, strlen(data));

    // 查找存在的模式
    size_t pos = ringbuffer_find(rb, "World", 5);
    assert(pos == 10); // "HelloWorld" 共10字符

    // 查找不存在的模式
    pos = ringbuffer_find(rb, "XYZ", 3);
    assert(pos == 0);

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例7：获取连续内存块
void test_contiguous_block() {
    printf("Test 7: Getting contiguous block\n");
    ringbuffer_t* rb = ringbuffer_create(8);
    assert(rb != NULL);

    // 写入数据
    ringbuffer_write(rb, "ABCD", 4);

    uint8_t* data_ptr;
    size_t contiguous_len = ringbuffer_get_contiguous(rb, &data_ptr);
    assert(contiguous_len == 4);
    assert(memcmp(data_ptr, "ABCD", 4) == 0);

    ringbuffer_destroy(rb);
    printf("Passed\n\n");
}

// 测试用例8：边界情况
void test_edge_cases() {
    printf("Test 8: Edge cases\n");
    // 创建大小为0的缓冲区（应该调整为最小大小）
    ringbuffer_t* rb = ringbuffer_create(0);
    assert(rb != NULL);
    assert(rb->size >= 1);
    ringbuffer_destroy(rb);

    // 测试无效参数
    assert(ringbuffer_write(NULL, "test", 4) == 0);
    assert(ringbuffer_read(NULL, NULL, 0) == 0);

    printf("Passed\n\n");
}

int main() {
    printf("Starting ringbuffer tests...\n\n");

    test_basic_write_read();
    test_wrap_around();
    test_full_write();
    test_empty_read();
    test_clear();
    test_find_pattern();
    test_contiguous_block();
    test_edge_cases();

    printf("All tests passed!\n");
    return 0;
}