#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

void serial_init(void);
void serial_puts(const char *s);
void print_hex(uint32_t v);
void serial_putc(char c);

#endif
