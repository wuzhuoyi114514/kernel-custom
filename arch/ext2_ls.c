#include <stdint.h>
#include "../include/ext2.h"
#include "../include/debug.h"
#include "../include/vga.h"
#include "../include/fs_runtime.h"
#include "../include/memory.h"

void ext2_ls(uint32_t dir_inode_num) {
    dbg_kv("ls", "dir_inode", dir_inode_num);
    struct ext2_inode inode;

    if (fs_gdt == 0) {
        dbg_msg("ls", "fs_gdt not initialized");
        vga_puts("Error: File system GDT not initialized.\n");
        return;
    }

    read_inode(dir_inode_num, g_sb, fs_gdt, &inode);

    if ((inode.i_mode & 0xF000) != 0x4000) {
        dbg_kv("ls", "not_directory_mode", inode.i_mode);
        vga_puts("Error: Inode is not a directory.\n");
        return;
    }

    __attribute__((aligned(8))) static uint8_t dir_block_buf[4096];

    uint32_t block_count = (inode.i_size + g_block_size - 1) / g_block_size;
    if (block_count == 0) return;

    uint8_t *indirect_buf = NULL;

    for (uint32_t blk = 0; blk < block_count; blk++) {
        uint32_t block_id = 0;

        if (blk < 12) {
            block_id = inode.i_block[blk];
        } else {
            if (inode.i_block[12] == 0) break;
            if (!indirect_buf) {
                indirect_buf = kmalloc(g_block_size);
                if (!indirect_buf) break;
                read_fs_block(inode.i_block[12], indirect_buf);
            }
            uint32_t *entries = (uint32_t *)indirect_buf;
            uint32_t idx = blk - 12;
            if (idx >= g_block_size / 4) break;
            block_id = entries[idx];
        }

        if (block_id == 0) continue;

        read_fs_block(block_id, dir_block_buf);

        uint32_t offset = 0;
        while (offset < g_block_size) {
            struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(dir_block_buf + offset);

            if (entry->rec_len == 0 || entry->rec_len > g_block_size) {
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
                serial_puts("[ls] entry=");
                serial_puts(name_buf);
                serial_puts("\n");
            }

            offset += entry->rec_len;
        }
    }

    if (indirect_buf) kfree(indirect_buf);

    dbg_msg("ls", "completed");
}
