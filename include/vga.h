#ifndef VGA_H
#define VGA_H

#define COLOR_BLACK          0
#define COLOR_BLUE           1
#define COLOR_GREEN          2
#define COLOR_CYAN           3
#define COLOR_RED            4
#define COLOR_MAGENTA        5
#define COLOR_BROWN          6
#define COLOR_LIGHT_GRAY     7
#define COLOR_DARK_GRAY      8
#define COLOR_LIGHT_BLUE     9
#define COLOR_LIGHT_GREEN    10
#define COLOR_LIGHT_CYAN     11
#define COLOR_LIGHT_RED      12
#define COLOR_LIGHT_MAGENTA  13
#define COLOR_YELLOW         14
#define COLOR_WHITE          15


#include <stdint.h>

void update_hardware_cursor(void);
void clear_screen(void);
void vga_puts(const char* str);
void vga_putc(char c);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_reset_color(void);

#endif
