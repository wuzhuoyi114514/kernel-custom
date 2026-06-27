#include <stdint.h>  // Freestanding 环境下 GCC 依然提供此头文件

#define VGA_ADDRESS     0xB8000
#define VGA_WIDTH        80
#define VGA_HEIGHT       25
#define DEFAULT_COLOR    0x0F  // 默认黑底高亮白字

// ==================== 【新增】VGA 标准 16 色常量定义 ====================
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

// 声明你在汇编或 io.h 中实现的端口输出函数
extern void outb(uint16_t port, uint8_t val);

// 使用 2D 坐标比一维计数器更能完美处理退格和换行
static int cursor_x = 0;
static int cursor_y = 0;

// 【新增】全局当前颜色属性，初始化为默认的黑底白字
static uint8_t g_current_color = DEFAULT_COLOR;

/**
 * 【新增接口】自由切换接下来的打字颜色
 * @param fg 前景色 (0~15)
 * @param bg 背景色 (0~15)
 */
void vga_set_color(uint8_t fg, uint8_t bg) {
    g_current_color = (bg << 4) | (fg & 0x0F);
}

/**
 * 【新增接口】重置回默认颜色
 */
void vga_reset_color(void) {
    g_current_color = DEFAULT_COLOR;
}

/**
 * 1. 更新显卡硬件光标
 */
void vga_update_cursor() {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;

    // 向 VGA 控制器写入低 8 位位置
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    // 向 VGA 控制器写入 high 8 位位置
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/**
 * 2. 滚屏函数：当写满 25 行时，整个屏幕往上推一行
 */
void vga_scroll() {
    uint16_t *video_mem = (uint16_t *)VGA_ADDRESS;
    
    // 将第 1~24 行的内容复制到第 0~23 行
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        video_mem[i] = video_mem[i + VGA_WIDTH];
    }
    
    // 清空最后一行（填入空格，使用当前设定的颜色样式）
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        video_mem[i] = (g_current_color << 8) | ' ';
    }
    
    cursor_y = VGA_HEIGHT - 1; // 光标固定在最后一行
}

/**
 * 3. 清屏函数
 */
void clear_screen() {
    uint16_t *vga_buffer = (uint16_t*)VGA_ADDRESS;
    for (int i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        // 使用当前颜色清屏，允许实现整体更换背景色
        vga_buffer[i] = (g_current_color << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

/**
 * 4. 打印单个字符
 */
void vga_putc(char c) {
    uint16_t *video_mem = (uint16_t *)VGA_ADDRESS;

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } 
    else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        }
    } 
    else {
        int offset = cursor_y * VGA_WIDTH + cursor_x;
        // 【核心修改】将固定的 DEFAULT_COLOR 替换为动态的 g_current_color
        video_mem[offset] = (g_current_color << 8) | c;
        cursor_x++;

        // 满 80 列自动换行
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    // 超出 25 行触发滚屏
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }

    // 每次打字完毕，更新硬件光标位置
    vga_update_cursor();
}

/**
 * 5. 打印字符串
 */
void vga_puts(const char* str) {
    int i = 0;
    while (str[i] != '\0') {
        vga_putc(str[i]); 
        i++;
    }
}

void vga_print_hex(uint32_t val) {
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        vga_putc(hex_chars[(val >> i) & 0xF]);
    }
}
