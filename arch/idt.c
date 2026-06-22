#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// ==================== 1. 基础结构体定义 ====================
struct idt_entry {
    unsigned short base_lo;   // 处理函数地址的低 16 位
    unsigned short sel;       // 内核代码段选择子
    unsigned char  always0;   // 永远为 0
    unsigned char  flags;     // 属性标志 (P, DPL, Type)
    unsigned short base_hi;   // 处理函数地址的高 16 位
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;     // 表界限 (总字节数 - 1)
    unsigned int base;        // 表基地址
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idt_ptr;

// ==================== 2. 底层内联汇编函数（必须放在最上面！） ====================
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// ==================== 3. 重新映射 8259A PIC 芯片（仅保留一份） ====================
void pic_remap() {
    // 初始化主、从 PIC
    outb(PIC1_COMMAND, 0x11);
    outb(0xA0, 0x11);

    // 把主 PIC 的 IRQ0~7 映射到中断向量 0x20~0x27 (键盘 IRQ1 对应 0x21)
    outb(PIC1_DATA, 0x20);
    // 把从 PIC 的 IRQ8~15 映射到中断向量 0x28~0x2F
    outb(0xA1, 0x28);

    // 级联设置
    outb(PIC1_DATA, 0x04);
    outb(0xA1, 0x02);

    // 设置为 8086 模式
    outb(PIC1_DATA, 0x01);
    outb(0xA1, 0x01);

    // 默认屏蔽所有硬件中断 (0xFF)
    outb(PIC1_DATA, 0xFF);
    outb(0xA1, 0xFF);
}

// ==================== 4. 设置中断门的辅助函数 ====================
void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

// 声明外部的汇编键盘中断包装函数（需要在 boot.s 中实现）
extern void keyboard_handler_asm(void);
extern void syscall_handler_asm(void);

// ==================== 5. 初始化函数 ====================
void idt_init() {
    // 先用辅助函数把 256 个中断全部初始化为安全的 Present=0 状态
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0x08, 0); 
    }

    // 重新映射可编程中断控制器 (PIC)
    pic_remap();

    // 注册键盘中断到 0x21 号向量 (IRQ 1)
    // 0x8E = 10001110b -> Present=1, DPL=0, 32位中断门
    idt_set_gate(0x21, (unsigned int)keyboard_handler_asm, 0x08, 0x8E);

        // ==================== 【核心新增】 ====================
    // 注册系统调用到 0x80 号向量
    // 0xEE = 11101110b -> Present=1, DPL=3 (允许用户态调用), 32位中断门
    idt_set_gate(0x80, (unsigned int)syscall_handler_asm, 0x08, 0xEE);
    // ======================================================

    // 设置正确的 Limit 并用 lidt 加载
    idt_ptr.limit = (sizeof(struct idt_entry) * 256) - 1;
    idt_ptr.base  = (unsigned int)&idt;

    __asm__ __volatile__("lidt %0" : : "m"(idt_ptr));

    // 单独允许主 PIC 响应键盘中断 (IRQ 1)
    // 11111101b = 0xFD (仅清零第 1 位，即开放 IRQ 1)
    outb(PIC1_DATA, 0xFD); 
}
