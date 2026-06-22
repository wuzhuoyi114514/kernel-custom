#include <stdint.h>

#define COM1 0x3F8

// 基础的 I/O 操作封装
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 初始化串口
void serial_init() {
    outb(COM1 + 1, 0x00);    // 关闭中断
    outb(COM1 + 3, 0x80);    // 开启 DLAB (允许设置波特率)
    outb(COM1 + 0, 0x03);    // 设置波特率低位 (38400)
    outb(COM1 + 1, 0x00);    // 设置波特率高位
    outb(COM1 + 3, 0x03);    // 8 bit, 无校验, 1 stop bit
}

// 发送字符
// arch/serial.c
void serial_putc(char c) {
    // 假设 COM1 端口是 0x3F8
    // 等待传输队列为空
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

// 发送字符串
void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}
