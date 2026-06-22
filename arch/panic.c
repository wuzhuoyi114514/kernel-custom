// kernel.c
#include "../include/vga.h"
#include "../include/debug.h"


// 声明这个函数，方便其他地方调用
void panic(const char *msg) {
    // 1. 关闭中断，防止在打印过程中被其他中断干扰
    __asm__ __volatile__("cli");

    // 2. 清除屏幕，设置一个醒目的红色背景
    vga_set_color(COLOR_WHITE, COLOR_RED);
    clear_screen();

    // 3. 打印核心信息
    vga_puts("!!! KERNEL PANIC !!!\n\n");
    serial_puts("!!! KERNEL PANIC !!!\n");
    vga_puts("Message: ");
     serial_puts("Message:");
    vga_puts(msg);
    serial_puts(msg);
    vga_puts("\n\nSystem halted\n");
    serial_puts("\n\nSystem halted\n");

    // 4. 彻底挂起 CPU
    while (1) {
        __asm__ __volatile__("hlt");
    }
}
