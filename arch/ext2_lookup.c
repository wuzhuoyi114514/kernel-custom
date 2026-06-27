#include <stdint.h>
#include "../include/ext2.h"
#include "../include/debug.h"
#include "../include/fs_runtime.h"
#include "../include/string.h"
#include "../include/memory.h"

// 在指定目录 dir_inode_num 下，寻找名字为 name 的子项，返回它的 Inode 号
uint32_t ext2_lookup(uint32_t dir_inode_num, const char *name) {
    struct ext2_inode inode;
    read_inode(dir_inode_num, g_sb, fs_gdt, &inode);

    // 安全检查：必须是个目录
    if ((inode.i_mode & 0xF000) != 0x4000) return 0;

    __attribute__((aligned(8))) static uint8_t dir_block_buf[4096];

    uint32_t block_count = (inode.i_size + g_block_size - 1) / g_block_size;
    if (block_count == 0) return 0;

    uint8_t *indirect_buf = NULL;
    uint32_t result = 0;

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

            if (entry->rec_len == 0 || entry->rec_len > g_block_size) break;

            if (entry->inode != 0 && entry->name_len > 0) {
                int len = entry->name_len;
                char name_buf[256];
                for (int i = 0; i < len; i++) {
                    name_buf[i] = (char)entry->name[i];
                }
                name_buf[len] = '\0';

                if (strcmp(name_buf, name) == 0) {
                    result = entry->inode;
                    goto lookup_done;
                }
            }
            offset += entry->rec_len;
        }
    }

lookup_done:
    if (indirect_buf) kfree(indirect_buf);
    return result;
}
