#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

void serial_init(void);
void serial_puts(const char *s);
void print_hex(uint32_t v);
void serial_putc(char c);

static inline void dbg_prefix(const char *scope) {
    serial_puts("[");
    serial_puts(scope);
    serial_puts("] ");
}

static inline void dbg_msg(const char *scope, const char *msg) {
    dbg_prefix(scope);
    serial_puts(msg);
    serial_puts("\n");
}

static inline void dbg_hex(const char *scope, uint32_t value) {
    dbg_prefix(scope);
    print_hex(value);
    serial_puts("\n");
}

static inline void dbg_kv(const char *scope, const char *key, uint32_t value) {
    dbg_prefix(scope);
    serial_puts(key);
    serial_puts("=");
    print_hex(value);
    serial_puts("\n");
}

#endif
