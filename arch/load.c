#include "ext2.h"
#include <stdint.h>
#include <stdbool.h>
#include "debug.h"
#include "fs_runtime.h"
#include "memory.h"

bool load_file_to_memory(uint32_t inode_num, uint8_t *dest_addr) {
    dbg_kv("load", "inode", inode_num);
    struct ext2_inode inode;
    read_inode(inode_num, g_sb, fs_gdt, &inode);

    uint32_t total_blocks = (inode.i_size + g_block_size - 1) / g_block_size;

    if (inode.i_size == 0) {
        dbg_msg("load", "file is empty");
        return false;
    }
    dbg_kv("load", "file_size", inode.i_size);

    uint8_t *current_ptr = dest_addr;
    uint32_t blocks_loaded = 0;

    for (uint32_t i = 0; i < total_blocks && i < 12; i++) {
        if (inode.i_block[i] == 0) {
            memset(current_ptr, 0, g_block_size);
        } else {
            read_fs_block(inode.i_block[i], current_ptr);
        }
        current_ptr += g_block_size;
        blocks_loaded++;
    }

    if (blocks_loaded < total_blocks) {
        if (inode.i_block[12] == 0) {
            dbg_msg("load", "missing single indirect block");
            return false;
        }

        if (g_block_size > 4096) {
            dbg_msg("load", "unsupported block size");
            return false;
        }

        uint8_t indirect_block[4096];
        read_fs_block(inode.i_block[12], indirect_block);
        uint32_t *entries = (uint32_t *)indirect_block;
        uint32_t indirect_count = g_block_size / sizeof(uint32_t);

        for (uint32_t i = 0; i < indirect_count && blocks_loaded < total_blocks; i++) {
            if (entries[i] == 0) {
                memset(current_ptr, 0, g_block_size);
            } else {
                read_fs_block(entries[i], current_ptr);
            }
            current_ptr += g_block_size;
            blocks_loaded++;
        }
    }

    if (blocks_loaded < total_blocks) {
        dbg_msg("load", "file truncated by sparse blocks");
        return false;
    }

    dbg_msg("load", "load complete");
    return true;
}
