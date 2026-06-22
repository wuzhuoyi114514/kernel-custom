#include <stdint.h>
#include <stddef.h>
#include "ext2.h"
#include "ata.h"
#include "debug.h"
#include "keyboard.h"
#include "idt.h"
#include "vga.h"
#include "path.h"
#include "memory.h"
#include "string.h"
#include "fs_runtime.h"
#include "shell_state.h"
#include "io.h"
#include "panic.h"
#include "gdt.h"
#include "shell_api.h"

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x1B  // 0x18 | 3
#define GDT_USER_DATA   0x23  // 0x20 | 3
#define GDT_TSS         0x28  // TSS 的选择子
#define SHELL_BUF_SIZE 256
#define MAX_HISTORY 30  // 最多保存 30 条历史记录

static char g_history[MAX_HISTORY][SHELL_BUF_SIZE]; // 现在这里绝对合法了！
static int g_history_count = 0;
static int g_history_idx = -1;
uint32_t g_cwd_inode = 2;
struct ext2_group_desc *fs_gdt = 0;
struct ext2_superblock *g_sb = 0;  // 全局 superblock 指针


struct tss_entry {
  uint32_t prev_tss;
  uint32_t esp0;       // 当用户态切换回内核态时，内核栈的物理指针
  uint32_t ss0;        // 内核数据段选择子 (你的代码里是 0x10)
  uint32_t esp1, ss1, esp2, ss2, cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs, ldt;
  uint16_t trap, iomap_base;
} __attribute__((packed));


struct gdt_entry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t  base_middle;
  uint8_t  access;
  uint8_t  granularity;
  uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

struct gdt_entry gdt[6];
struct gdt_ptr gp;
struct tss_entry tss;

char g_cwd_path[128] = "/";

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
    uint8_t access, uint8_t gran)
{
  gdt[num].base_low    = base & 0xFFFF;
  gdt[num].base_middle = (base >> 16) & 0xFF;
  gdt[num].base_high   = (base >> 24) & 0xFF;

  gdt[num].limit_low   = limit & 0xFFFF;
  gdt[num].granularity = (limit >> 16) & 0x0F;

  gdt[num].granularity |= gran & 0xF0;
  gdt[num].access      = access;
}

void tss_init(uint32_t ss0, uint32_t esp0) {
  uint32_t base = (uint32_t)&tss;
  uint32_t limit = sizeof(tss) - 1;

  // 清空 TSS
  for(int i = 0; i < sizeof(tss); i++) ((char*)&tss)[i] = 0;

  tss.ss0 = ss0;
  tss.esp0 = esp0;
  tss.iomap_base = sizeof(tss); // 禁用 I/O 许可位图

  // 注册到 GDT 的第 5 号槽位 (0x28)，Access 属性为 0x89 (可用的 32位 TSS)
  gdt_set_gate(5, base, limit, 0x89, 0x00);
}

void gdt_init()
{
  gp.limit = sizeof(gdt) - 1;
  gp.base  = (uint32_t)&gdt;

  gdt_set_gate(0, 0, 0, 0, 0);                // Null
  gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Kernel Code (Ring 0)
  gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data (Ring 0)

  // 【新增】用户态代码段和数据段 (Access 属性 0xFA 和 0xF2 中的 'F' 代表 DPL=3)
  gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User Code (Ring 3)
  gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data (Ring 3)

  tss_init(GDT_KERNEL_DATA, 0x90000);
  gdt_flush((uint32_t)&gp);
  __asm__ __volatile__("movw $0x28, %ax; ltr %ax");
}

// 定义 VGA 缓冲区基地址
#define VGA_ADDRESS 0xB8000
// --- 外部声明 ---

void handle_command(char *cmd);
void shell_backspace(int *pos);

// --- 简单的显示驱动 ---

//syscall
//// 对应汇编里 pusha 和 push ds/es 后的栈结构
struct syscall_regs {
  // 对应汇编最后 push 的 ES 和 DS (栈顶)
  uint32_t es;
  uint32_t ds;

  // 对应 pusha 压入的 8 个通用寄存器 (EDI 是 pusha 最后压入的，在最上面)
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp_dummy; // pusha 压入的原始 ESP，不用管它
  uint32_t ebx;       // 对应用户态传进来的：字符串指针
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;       // 对应用户态传进来的：系统调用号

  // 以下是进入 int 0x80 时 CPU 自动压入的用户态现场 (栈底)
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t user_esp;
  uint32_t user_ss;
};
void c_syscall_handler(struct syscall_regs *regs) {
  // 约定：用 eax 传递系统调用号
  uint32_t syscall_num = regs->eax;
  serial_puts("DEBUG SYSCALL: Got EAX = "); 
  print_hex(syscall_num);
  serial_puts(", EBX = ");                 
  print_hex(regs->ebx);
  serial_puts("\n");
  switch (syscall_num) {
    case 1: {
              // 1 号系统调用：打印字符串
              // 约定：用 ebx 传递用户态字符串的地址
              char *str = (char *)regs->ebx;
              vga_puts(str); 
              break;
            }
    case 2: {
              // 2 号系统调用：清屏
              clear_screen();
              break;
            }
            // 在内核入口文件的 c_syscall_handler 中
    case 3: { // 假设 3 号系统调用是 sys_exit
              serial_puts("User program exited.\n");
              // 这里其实是一个非常高深的话题：如何恢复用户态前的内核栈？
              // 最简单粗暴的临时做法：在 shell 循环中利用 setjmp/longjmp 或者干脆重新调用一次 run_shell()
              // 但更推荐的做法是：让你的 asm_switch_to_user 采用 call 指令而非 jmp
              break;
            }
    default:
            vga_puts("Syscall Error: Unknown syscall number.\n");
            break;
  }
}

int strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') len++;
  return len;
}

char* strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++));
  return dest;
}

int strncmp(const char *a, const char *b, uint32_t n)
{
  while (n && *a && (*a == *b)) {
    a++;
    b++;
    n--;
  }
  if (n == 0) return 0;
  return *(unsigned char*)a - *(unsigned char*)b;
}

int strcmp(const char *a, const char *b)
{
  while (*a && (*a == *b)) {
    a++;
    b++;
  }
  return *(unsigned char*)a - *(unsigned char*)b;
}



void print_hex(uint32_t val) {
  // 11个字节的纯局部栈空间：'0', 'x', 8个十六进制符, '\0'
  char buf[11]; 

  buf[0] = '0';
  buf[1] = 'x';
  buf[10] = '\0'; // 提前焊死结尾，绝对不给 serial_puts 越界的机会

  const char *lookup = "0123456789ABCDEF";

  // 从右往左，剥离出 8 个半字节(nibble)
  for (int i = 9; i >= 2; i--) {
    buf[i] = lookup[val & 0x0F];
    val >>= 4;
  }

  serial_puts(buf);
}

void shell_backspace(int *pos)
{
  if (*pos <= 0) return;

  (*pos)--;

  serial_puts("\b \b"); // 覆盖字符
}
void run_shell()
{
  char input_buffer[SHELL_BUF_SIZE];
  int pos = 0;
  int len = 0; // 当前输入的字符串总长度
               //
               // 1. 统一使用 vga_puts 输出欢迎语和初始提示符
  vga_set_color(COLOR_YELLOW, COLOR_BLACK);
  vga_puts("Welcome to Kernel Shell \n");
  vga_set_color(COLOR_GREEN, COLOR_BLACK);
  vga_puts("Kernel:"); 

  // [/] 路径显示为优雅的青色
  vga_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
  vga_puts("/"); 

  // [> ] 符号切回纯白色，等待用户敲击
  vga_set_color(COLOR_WHITE, COLOR_BLACK);
  vga_puts("> ");


  while (1)
  {
    char c = keyboard_buffer_pop();

    if (c == 0)
    {
      __asm__("hlt");
      continue;
    }

    // ==================== 【1. ENTER 键处理】 ====================
    if (c == '\n')
    {
      input_buffer[pos] = '\0';

      // 统一改成 vga_puts，让屏幕换行
      vga_puts("\n"); 
      if (pos > 0) {
        if (g_history_count == 0 || strcmp(g_history[(g_history_count - 1) % MAX_HISTORY], input_buffer) != 0) {
          if (g_history_count < MAX_HISTORY) {
            strcpy(g_history[g_history_count], input_buffer);
            g_history_count++;
          } else {
            // 队列满了，整体向前滚动一行
            for (int i = 1; i < MAX_HISTORY; i++) {
              strcpy(g_history[i - 1], g_history[i]);
            }
            strcpy(g_history[MAX_HISTORY - 1], input_buffer);
          }
        }
      }
      g_history_idx = g_history_count; // 重置翻阅指针，指向最新空白行
                                       // 执行命令（确保你的命令处理器内部如果需要打印，也用 vga_puts）
      handle_command(input_buffer);
      pos = 0;
      len = 0;
      for (int i = 0; i < SHELL_BUF_SIZE; i++) {
        input_buffer[i] = '\0';
      }
      pos = 0;
      // 【关键修改】执行完后，在 VGA 屏幕上重新打印提示符，光标会自动跟过去
      vga_set_color(COLOR_GREEN, COLOR_BLACK);
      vga_puts("Kernel:"); 
      vga_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
      vga_puts(g_cwd_path); 
      vga_set_color(COLOR_WHITE, COLOR_BLACK);
      vga_puts("> ");   
    }
    // 2. 【核心功能】处理上键：翻阅上一条历史
    else if (c == 0x11) 
    {
      if (g_history_count > 0 && g_history_idx > 0) {
        g_history_idx--;

        // 【视觉核心】清除屏幕上当前已经输入的字符
        for (int i = 0; i < pos; i++) {
          vga_putc('\b'); vga_putc(' '); vga_putc('\b');
        }

        // 装载历史命令到缓冲区
        strcpy(input_buffer, g_history[g_history_idx]);
        pos = strlen(input_buffer);

        // 显示回屏幕上
        vga_puts(input_buffer);
      }
    }

    // 3. 【核心功能】处理下键：翻阅下一条历史
    else if (c == 0x12) 
    {
      if (g_history_idx < g_history_count) {
        g_history_idx++;

        // 清除当前屏幕输入
        for (int i = 0; i < pos; i++) {
          vga_putc('\b'); vga_putc(' '); vga_putc('\b');
        }

        if (g_history_idx == g_history_count) {
          // 翻到最下面了，恢复成空行
          input_buffer[0] = '\0';
          pos = 0;
        } else {
          // 装载下一条历史
          strcpy(input_buffer, g_history[g_history_idx]);
          pos = strlen(input_buffer);
          vga_puts(input_buffer);
        }
      }
    }
    // ==================== 【2. BACKSPACE 键处理】 ====================
    else if (c == '\b')
    {
      if (pos > 0) {
        pos--;
        // 将光标右侧的字符集体向左挪动一位
        for (int i = pos; i < len - 1; i++) {
          input_buffer[i] = input_buffer[i + 1];
        }
        len--;
        vga_putc('\b');
        for (int i = pos; i < len; i++) vga_putc(input_buffer[i]);
        vga_putc(' '); // 擦除原本末尾残留的最后一个字符

        // 将 VGA 物理光标退回到逻辑 pos 位置
        for (int i = 0; i < (len + 1 - pos); i++) {
          vga_putc('\b');
        }
      }
    }
    // 4. 【新增】处理左键：光标左移
    else if (c == 0x13) 
    {
      if (pos > 0) {
        pos--;
        vga_putc('\b'); // VGA 驱动的 \b 只移动光标，不擦除字符
      }
    }

    // 5. 【新增】处理右键：光标右移
    else if (c == 0x14) 
    {
      if (pos < len) {
        vga_putc(input_buffer[pos]); // 通过重新打印当前字符让光标向右走
        pos++;
      }
    }
    // ==================== 【3. 普通字符处理】 ====================
    else
    {
      if (len < SHELL_BUF_SIZE - 1) {
        // 将光标右侧的字符集体向右挪动一位，腾出空位
        for (int i = len; i > pos; i--) {
          input_buffer[i] = input_buffer[i - 1];
        }
        input_buffer[pos] = c;
        len++;

        // 从当前位置往后重新渲染整行文本
        for (int i = pos; i < len; i++) {
          vga_putc(input_buffer[i]);
        }
        pos++;

        // 将 VGA 物理光标送回到逻辑 pos 位置
        for (int i = len; i > pos; i--) {
          vga_putc('\b');
        }
      }
    }
  }
}

// 声明刚才写好的外部汇编函数
extern void asm_switch_to_user(uint32_t entry_point, uint32_t user_stack_top);
static const char *user_msg = "Hello from Ring 3 Syscall!\n";

void run_user_program_at(uint32_t entry_point) {
  uint32_t user_stack_top = USER_STACK_TOP;

  // 极其重要：获取当前干净的内核栈指针给 TSS.esp0
  // 这样当用户程序触发 int 0x80 时，CPU 才知道切回哪里
  uint32_t current_kernel_esp;
  __asm__ __volatile__("mov %%esp, %0" : "=r"(current_kernel_esp));
  tss.esp0 = current_kernel_esp; 

  // 调用纯汇编函数，彻底告别递归和 GCC 内联优化背刺
  asm_switch_to_user(entry_point, user_stack_top);
}

// ============ 【新增】用户程序运行函数 ============
// 从文件系统中加载并执行用户程序
bool run_user_program_from_file(const char *filepath) {
  vga_puts("[DEBUG] Attempting to load: ");
  vga_puts(filepath);
  vga_puts("\n");

  // 1. 解析路径获取 inode
  uint32_t inode_num = resolve_path(filepath);
  
  if (inode_num == 0) {
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("Error: File not found - ");
    vga_puts(filepath);
    vga_puts("\n");
    vga_reset_color();
    return false;
  }

  // 2. 读取 inode 元数据，验证是否为常规文件
  struct ext2_inode file_inode;
  read_inode(inode_num, g_sb, fs_gdt, &file_inode);

  uint16_t file_type = file_inode.i_mode & 0xF000;
  if (file_type == 0x4000) {
    // 0x4000 是目录类型
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("Error: ");
    vga_puts(filepath);
    vga_puts(" is a directory, not an executable\n");
    vga_reset_color();
    return false;
  }

  if (file_type != 0x8000) {
    // 0x8000 是常规文件类型
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("Error: Invalid file type (mode=0x");
    print_hex(file_inode.i_mode);
    vga_puts(")\n");
    vga_reset_color();
    return false;
  }

  // 3. 检查文件大小
  uint32_t file_size = file_inode.i_size;
  vga_puts("[DEBUG] File size: ");
  print_hex(file_size);
  vga_puts(" bytes\n");

  if (file_size == 0) {
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("Error: File is empty\n");
    vga_reset_color();
    return false;
  }

  if (file_size > USER_PROGRAM_MAX_SIZE) {
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("Error: File too large (max ");
    print_hex(USER_PROGRAM_MAX_SIZE);
    vga_puts(" bytes)\n");
    vga_reset_color();
    return false;
  }

  // 4. 加载文件到用户空间
  vga_puts("[DEBUG] Loading to 0x");
  print_hex(USER_PROGRAM_BASE);
  vga_puts("...\n");

  paging_mark_user_range(USER_PROGRAM_BASE, (file_size + 0xFFFu) & ~0xFFFu);

  if (!load_file_to_memory(inode_num, (uint8_t *)USER_PROGRAM_BASE)) {
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("Error: Failed to load file into memory\n");
    vga_reset_color();
    return false;
  }

  // 5. 执行用户程序
  vga_set_color(COLOR_GREEN, COLOR_BLACK);
  vga_puts("[INFO] Launching user program at 0x");
  print_hex(USER_PROGRAM_BASE);
  vga_puts("\n");
  vga_reset_color();

  run_user_program_at(USER_PROGRAM_BASE);
  
  // 用户程序执行完后回到这里（如果采用了 call 而非 jmp）
  vga_set_color(COLOR_YELLOW, COLOR_BLACK);
  vga_puts("[INFO] User program returned\n");
  vga_reset_color();
  
  return true;
}

static uint8_t user_code_test[] = { 0xB0, 'A', 0xB4, 0x0E, 0xEB, 0xFC };

void handle_command(char *cmd)
{
  if (strcmp(cmd, "help") == 0) {
    vga_puts("commands: help, ls, clear, cd, ./program\n");
    vga_puts("  - help            Show this help message\n");
    vga_puts("  - ls [path]       List directory contents\n");
    vga_puts("  - cd <path>       Change directory\n");
    vga_puts("  - clear           Clear screen\n");
    vga_puts("  - ./program       Execute program from current directory\n");
    vga_puts("  - /path/program   Execute program from absolute path\n");
  }
  else if (strcmp(cmd, "test_ring3") == 0) {
    vga_puts("Switching to Ring 3...\n");

    // 静态数组存放硬编码机器码
    static uint8_t user_program[32];

    user_program[0] = 0xB8; 
    *(uint32_t*)&user_program[1] = 1;                  // mov eax, 1

    user_program[5] = 0xBB; 
    *(uint32_t*)&user_program[6] = (uint32_t)user_msg;    // mov ebx, &user_msg

    user_program[10] = 0xCD; 
    user_program[11] = 0x80;                           // int 0x80

    user_program[12] = 0xEB; 
    user_program[13] = 0xFE;                           // jmp $

    vga_puts("Launching program with syscall support...\n");
    run_user_program_at((uint32_t)user_program);
  }
  else if (strcmp(cmd, "cat") == 0) {
    vga_puts("cat: Missing filename.\n");
  }
  else if (strcmp(cmd, "cd") == 0) {
    // 现代 Linux 中单独敲 cd 会回到主目录。
    // 在我们的简易内核里，没有用户主目录的概念，所以直接 return（原地不动）是最安全的做法。
    return; 
  }
  else if (strncmp(cmd, "cd ", 3) == 0) {
    // 【新功能】截取 cd 后面的目录名（比如 "cd dev" -> "dev"）
    char *path = cmd + 3; 
    ext2_cd(path);
  }
  else if (strncmp(cmd, "cat ", 4) == 0) {
    char *filename = cmd + 4;
    ext2_cat(filename);
  }
  else if (strcmp(cmd, "ls") == 0) {
    // 1. 无参数：显示当前工作目录
    ext2_ls(g_cwd_inode);
  }
  else if (strncmp(cmd, "ls ", 3) == 0) {
    // 2. 带参数：解析路径，显示指定目录
    char *path = cmd + 3; // 获取空格后的路径字符串

    uint32_t target_inode = resolve_path(path);

    if (target_inode != 0) {
      // 【关键】检查目标 Inode 是否确实是一个目录
      struct ext2_inode inode_data;
      read_inode(target_inode, g_sb, fs_gdt, &inode_data);

      if ((inode_data.i_mode & 0xF000) == 0x4000) {
        ext2_ls(target_inode);
      } else {
        vga_puts("ls: ");
        vga_puts(path);
        vga_puts(": Not a directory\n");
      }
    } else {
      vga_puts("ls: No such file or directory\n");
    }
  }
  else if (strcmp(cmd, "clear") == 0) {
    clear_screen();
  }
  else if (cmd[0] == 0) {
    return;
  }
  // ============ 【新增】用户程序执行处理 ============
  else if (cmd[0] == '/' || strncmp(cmd, "./", 2) == 0) {
    // 绝对路径或相对路径执行程序
    run_user_program_from_file(cmd);
  }
  else {
    vga_set_color(COLOR_WHITE, COLOR_RED);
    vga_puts("unknown command: ");
    vga_puts(cmd);
    vga_puts("\n");
    vga_reset_color();
  }
}

// 定义一个足够大的全局缓冲区，防止栈溢出
// 定义独立缓冲区，彻底隔离元数据
void kmain(uint32_t mb_magic, uint32_t mb_info_addr) {
   *(uint16_t*)0xB8000 = 0x4F41; 
  if (!ata_init()) {
        panic("ATA subsystem failed to initialize.");
    }
  serial_init();
  clear_screen();
  gdt_init();
  idt_init();
  memory_init(mb_magic, mb_info_addr);
  serial_puts("--- Kernel Booting ---\n");
  __asm__ __volatile__("sti");


  /* * 1. 使用独立的、且强制 8 字节对齐的缓冲区
   * 将 gdt_buffer 直接开到 4KB，防御 4096 block_size 的背刺
   */
  __attribute__((aligned(8))) static uint8_t sb_buffer[1024]; 
  __attribute__((aligned(8))) static uint8_t gdt_buffer[4096]; 

  // 2. 读取超级块 (注意：此处硬编码 LBA 2 仅适用于无分区表的 Raw Image)
  probe_ext2_partition();            // 1. 先动态探测分区
  disk_read(ext2_start_lba + 2, sb_buffer, 2); // 2. 相对偏移 2 读出超级块
  serial_puts("\nGot Superblock!!!\n");
  struct ext2_superblock *sb = (struct ext2_superblock *)sb_buffer;

  if (sb->s_magic == 0xEF53) {
    serial_puts("FS INIT\n");
    fs_init((struct ext2_superblock *)sb_buffer); // 3. 传入你的 fs_init 算大小 
                                                  // --- 在这里添加 ---
    serial_puts("DEBUG: --- FS Meta Data ---\n");
    serial_puts("Block Size: "); print_hex(1024 << sb->s_log_block_size); // 真正的字节数
    serial_puts("First Data Block: "); print_hex(sb->s_first_data_block);
    serial_puts("Inodes per group: "); print_hex(sb->s_inodes_per_group);
    serial_puts("BLOCK SIZE FINAL: ");
    print_hex(g_block_size);
    serial_puts("\n");
    //serial_puts("DEBUG: Dumping raw disk sectors from LBA 2...\n");
    //uint8_t dump_buf[512];

    // 直接读 LBA 2, 3, 4, 5，不通过 read_fs_block，直接用 disk_read
    //for (uint32_t lba = 2; lba < 6; lba++) {
    //  disk_read(lba, dump_buf, 1);
    // serial_puts("LBA "); print_hex(lba); serial_puts(": ");
    // for (int i = 0; i < 16; i++) { // 只打前16字节观察特征
    //     print_hex(dump_buf[i]);
    //     serial_puts(" ");
    // }
    // serial_puts("\n");
    //}
    // 1. 根据超级块确定 GDT 所在的块
    uint32_t fs_gdt_block_id = sb->s_first_data_block + 1; 

    // 2. 严谨地读取该块
    read_fs_block(fs_gdt_block_id, gdt_buffer);

    // 3. 打印出完整的 GDT 区域，观察是否存在魔数或有效特征
    fs_gdt = (struct ext2_group_desc *)gdt_buffer; 
    g_sb = sb;  // ✅ 保存全局 superblock 指针
    // ====== 终极抓贼打印 A ======
    serial_puts("\n[AM_INIT] Pointer Variable Address (&fs_gdt) = "); 
    print_hex((uint32_t)&fs_gdt);
    serial_puts("\n[AM_INIT] Pointer Inside Value     (fs_gdt)  = "); 
    print_hex((uint32_t)fs_gdt);
    serial_puts("\n");
    // ==========================
    serial_puts("DEBUG: GDT[0].bg_inode_table = "); 
    print_hex(fs_gdt[0].bg_inode_table); 
    serial_puts("\n");
    serial_puts("DEBUG: Validating GDT[0]...\n");
    serial_puts("bg_inode_table: "); print_hex(fs_gdt[0].bg_inode_table); serial_puts("\n");

    if (fs_gdt[0].bg_inode_table == 0 || fs_gdt[0].bg_inode_table > 0x10000) {
      serial_puts("FATAL: GDT corrupted or wrong buffer offset!\n");
      while(1);
    }
    serial_puts("\n");

    // 4. 读取根目录 Inode
    struct ext2_inode root_inode;
    read_inode(2, sb, fs_gdt, &root_inode);  // ✅ 修复：正确的参数顺序 (inode_num, superblock, gdt, out)

    // 5. 严谨的目录类型判定
    if ((root_inode.i_mode & 0xF000) == 0x4000) {
      serial_puts("Directory confirmed.\n");
      //最后一步

      // 填 ISR 表！！！

      //      __asm__ volatile("sti");   // ⭐ 最后一步

      run_shell();
    } else {
      // 调试：打印出 i_mode 的原始值，看看它到底是多少
      serial_puts("FATAL: Inode 2 i_mode is: ");
      panic("Filesystem Error");
      print_hex(root_inode.i_mode);
      serial_puts("\n");
    }
  } else {
    panic("Error: Not an Ext2 filesystem (Magic mismatch).");
    serial_puts("Error: Not an Ext2 filesystem (Magic mismatch).\n");
  }

  while (1);
}
