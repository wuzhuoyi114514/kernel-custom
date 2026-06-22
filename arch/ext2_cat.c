#include <stdint.h>
#include "ext2.h"
#include "debug.h"
#include "vga.h"
#include "fs_runtime.h"
#include "shell_state.h"

extern uint32_t ext2_lookup(uint32_t dir_inode_num, const char *name);

void ext2_cat(const char *filename) {
    dbg_msg("cat", "cat request");
    if (filename == 0 || filename[0] == '\0') {
        dbg_msg("cat", "missing filename");
        vga_puts("cat: Missing filename.\n");
        return;
    }
    serial_puts("[cat] filename=");
    serial_puts(filename);
    serial_puts("\n");

    // 1. 获取真实的 Inode 号
    uint32_t file_inode_num = ext2_lookup(g_cwd_inode, filename);
    if (file_inode_num == 0) {
        dbg_msg("cat", "file not found");
        vga_puts("cat: File not found.\n");
        return;
    }

    // 2. 读取 Inode 结构体
    struct ext2_inode inode;
    read_inode(file_inode_num, g_sb, fs_gdt, &inode);

    // 3. 【核心修改】明确区分：如果是目录(0x4000)报错，如果不是普通文件(0x8000)报错
    if ((inode.i_mode & 0xF000) == 0x4000) {
        dbg_kv("cat", "is_directory_mode", inode.i_mode);
        vga_puts("cat: Cannot cat a directory.\n");
        return;
    }
    if ((inode.i_mode & 0xF000) != 0x8000) {
        dbg_kv("cat", "invalid_mode", inode.i_mode);
        vga_puts("cat: Not a regular file.\n");
        return;
    }

    // 4. 打印数据块内容（保持原样）
    uint32_t size_remaining = inode.i_size;
    if (size_remaining == 0) return; 
    dbg_kv("cat", "file_size", size_remaining);

    __attribute__((aligned(8))) static uint8_t block_buf[4096];
    
    for (int i = 0; i < 12 && size_remaining > 0; i++) {
        uint32_t block_id = inode.i_block[i];
        if (block_id == 0) break;

        read_fs_block(block_id, block_buf);
        dbg_kv("cat", "block_id", block_id);

        uint32_t bytes_to_read = g_block_size;
        if (size_remaining < g_block_size) {
            bytes_to_read = size_remaining;
        }

        for (uint32_t j = 0; j < bytes_to_read; j++) {
            vga_putc((char)block_buf[j]);
        }
        size_remaining -= bytes_to_read;
    }
    vga_putc('\n'); 
}
