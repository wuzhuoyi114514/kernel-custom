#include <stdint.h>
#include "../include/debug.h"
#include "../include/io.h"
extern void serial_putc(char c);

static const char scan_code_map_normal[128] = {
    0,  0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static const char scan_code_map_shift[128] = {
    0,  0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static int g_lshift_down = 0;
static int g_rshift_down = 0;
static int g_lctrl_down = 0;
static int g_rctrl_down = 0;
static int g_lalt_down = 0;
static int g_ralt_down = 0;
static int g_is_e0 = 0;
static int g_key_down[128] = {0};

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

void keyboard_handler(void) {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);

        if (scancode == 0xE0) {
            g_is_e0 = 1;
        }
        else if (g_is_e0) {
            g_is_e0 = 0;
            if (scancode == 0x48) { keyboard_buffer_push(0x11); }
            else if (scancode == 0x50) { keyboard_buffer_push(0x12); }
            else if (scancode == 0x4B) { keyboard_buffer_push(0x13); }
            else if (scancode == 0x4D) { keyboard_buffer_push(0x14); }
            else if (scancode == 0x1D) { g_rctrl_down = 1; }
            else if (scancode == 0x9D) { g_rctrl_down = 0; }
            else if (scancode == 0x38) { g_ralt_down = 1; }
            else if (scancode == 0xB8) { g_ralt_down = 0; }
        }
        else {
            if (scancode == 0x2A) { g_lshift_down = 1; }
            else if (scancode == 0x36) { g_rshift_down = 1; }
            else if (scancode == 0xAA) { g_lshift_down = 0; }
            else if (scancode == 0xB6) { g_rshift_down = 0; }
            else if (scancode == 0x1D) { g_lctrl_down = 1; }
            else if (scancode == 0x9D) { g_lctrl_down = 0; }
            else if (scancode == 0x38) { g_lalt_down = 1; }
            else if (scancode == 0xB8) { g_lalt_down = 0; }
            else if (scancode < 0x80) {
                if (!g_key_down[scancode]) {
                    g_key_down[scancode] = 1;
                    int shift = g_lshift_down || g_rshift_down;
                    int ctrl = g_lctrl_down || g_rctrl_down;
                    char c = shift ? scan_code_map_shift[scancode] : scan_code_map_normal[scancode];
                    if (ctrl && c >= 'a' && c <= 'z') {
                        c = c - 'a' + 1;
                    } else if (ctrl && c >= 'A' && c <= 'Z') {
                        c = c - 'A' + 1;
                    }
                    if (c != 0) {
                        keyboard_buffer_push(c);
                    }
                }
            } else {
                int key = scancode - 0x80;
                if (key < 128) {
                    g_key_down[key] = 0;
                }
            }
        }
    }

    outb(0x20, 0x20);
}
