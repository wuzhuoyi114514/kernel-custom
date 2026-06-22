#include <stdint.h>
#include "../include/io.h"
extern void serial_putc(char c); 

// 1. 正常未按下 Shift 的映射表
static const char scan_code_map_normal[128] = {
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

// 2. 按下 Shift 之后的映射表
static const char scan_code_map_shift[128] = {
    0,  0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

// 状态机变量
static int g_shift_pressed = 0;
static int g_is_e0 = 0; 

// 环形缓冲区
#define KBD_BUF_SIZE 128
static volatile int head = 0;
static volatile int tail = 0;
static char kbd_buffer[KBD_BUF_SIZE];

void keyboard_buffer_push(char c) {
    int next = (head + 1) % KBD_BUF_SIZE;
    if (next != tail) {
        kbd_buffer[head] = c;
        head = next;
    }
}

char keyboard_buffer_pop(void) {
    if (head == tail) return 0;
    char c = kbd_buffer[tail];
    tail = (tail + 1) % KBD_BUF_SIZE;
    return c;
}

// 中断服务程序
void keyboard_handler(void) {
    uint8_t status = inb(0x64);
    if (status & 0x01) { 
        uint8_t scancode = inb(0x60);
        
        // 分支 1：处理 E0 前缀
        if (scancode == 0xE0) {
            g_is_e0 = 1;
        } 
        // 分支 2：处理紧跟在 E0 后面的方向键
        else if (g_is_e0) {
            g_is_e0 = 0; // 重置状态
            
            if (scancode == 0x48) {        // 方向键【上】
                keyboard_buffer_push(0x11); // 发射自定义的上键信号
            } 
            else if (scancode == 0x50) {   // 方向键【下】
                keyboard_buffer_push(0x12); // 发射自定义的下键信号
            }
                else if (scancode == 0x4B) {   // 【新增】方向键【左】
                keyboard_buffer_push(0x13); // 发射自定义的左键信号
            }
            else if (scancode == 0x4D) {   // 【新增】方向键【右】
                keyboard_buffer_push(0x14); // 发射自定义的右键信号
            }
        } 
        // 分支 3：普通的标准键与 Shift 状态机
        else {
            if (scancode == 0x2A || scancode == 0x36) {
                g_shift_pressed = 1;
            } 
            else if (scancode == 0xAA || scancode == 0xB6) {
                g_shift_pressed = 0;
            }
            else if (scancode < 0x80) {
                if (scancode < 128) {
                    char c = g_shift_pressed ? scan_code_map_shift[scancode] : scan_code_map_normal[scancode];
                    if (c != 0) {
                        keyboard_buffer_push(c);
                        serial_putc(c); 
                    }
                }
            }
        }
    } // 这里闭合 if (status & 0x01)

    // 发送 EOI，必须在键盘处理函数体内！
    outb(0x20, 0x20);
} // 这里完美闭合 keyboard_handler
