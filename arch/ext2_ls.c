#include <stdint.h>
#include "../include/fs.h"
#include "../include/ext2.h"
#include "../include/debug.h"
#include "../include/vga.h"

// 【修改这里】精确匹配 kernel.c 中的定义，用 extern 引用
extern struct ext2_group_desc *fs_gdt;
extern uint32_t g_block_size;


void ext2_ls(uint32_t dir_inode_num) {
      // ====== 终极抓贼打印 B ======
    serial_puts("\n[AT_SHELL] Pointer Variable Address (&fs_gdt) = "); 
    print_hex((uint32_t)&fs_gdt);
    serial_puts("\n[AT_SHELL] Pointer Inside Value     (fs_gdt)  = "); 
    print_hex((uint32_t)fs_gdt);
    serial_puts("\n");
    // ==========================
    struct ext2_inode inode;
    
    // 安全检查
    if (fs_gdt == 0) {
      serial_puts("LS side &fs_gdt = "); print_hex((uint32_t)&fs_gdt); serial_puts("\n");
        vga_puts("Error: File system GDT not initialized.\n");
        return;
    }
    
    // 1. 读取指定的 Inode 结构体（类型完美匹配，不再需要强转！）
    read_inode(dir_inode_num, fs_gdt, &inode);

    // 2. 安全检查：确保它是一个目录
    if ((inode.i_mode & 0xF000) != 0x4000) {
        vga_puts("Error: Inode is not a directory.\n");
        return;
    }

    __attribute__((aligned(8))) static uint8_t dir_block_buf[4096];
    read_fs_block(inode.i_block[0], dir_block_buf);

    uint32_t offset = 0;
    while (offset < g_block_size) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(dir_block_buf + offset);

        if (entry->rec_len == 0) {
            break;
        }

        if (entry->inode != 0 && entry->name_len > 0) {
            char name_buf[256];
            int len = entry->name_len;
            if (len > 255) len = 255;
            
            for (int i = 0; i < len; i++) {
                name_buf[i] = (char)entry->name[i];
            }
            name_buf[len] = '\0';

            if (entry->file_type == 2) {
                vga_puts("<DIR> ");
            } else {
                vga_puts("      ");
            }

            vga_puts(name_buf);
            vga_puts("\n");
        }

        offset += entry->rec_len;
    }
}
