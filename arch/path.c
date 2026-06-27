#include "path.h"
#include "ext2.h"
#include "string.h"
#include "fs_runtime.h"
#include "shell_state.h"
#include "memory.h"

// 查找特定目录下的条目
uint32_t find_inode_in_dir(uint32_t parent_inode, const char *name) {
    struct ext2_inode dir_inode;
    read_inode(parent_inode, g_sb, fs_gdt, &dir_inode);

    if ((dir_inode.i_mode & 0xF000) != 0x4000) return 0;

    __attribute__((aligned(8))) static uint8_t block_buf[4096];

    uint32_t block_count = (dir_inode.i_size + g_block_size - 1) / g_block_size;
    if (block_count == 0) return 0;

    uint8_t *indirect_buf = NULL;
    uint32_t result = 0;

    for (uint32_t blk = 0; blk < block_count; blk++) {
        uint32_t block_id = 0;

        if (blk < 12) {
            block_id = dir_inode.i_block[blk];
        } else {
            if (dir_inode.i_block[12] == 0) break;
            if (!indirect_buf) {
                indirect_buf = kmalloc(g_block_size);
                if (!indirect_buf) break;
                read_fs_block(dir_inode.i_block[12], indirect_buf);
            }
            uint32_t *entries = (uint32_t *)indirect_buf;
            uint32_t idx = blk - 12;
            if (idx >= g_block_size / 4) break;
            block_id = entries[idx];
        }

        if (block_id == 0) continue;

        read_fs_block(block_id, block_buf);

        uint32_t offset = 0;
        while (offset < g_block_size) {
            struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(block_buf + offset);
            if (entry->rec_len == 0 || entry->rec_len > g_block_size) break;

            if (entry->inode != 0 && entry->name_len > 0) {
                if (strncmp(entry->name, name, entry->name_len) == 0 && entry->name_len == strlen(name)) {
                    result = entry->inode;
                    goto done;
                }
            }
            offset += entry->rec_len;
        }
    }

done:
    if (indirect_buf) kfree(indirect_buf);
    return result;
}

// 主解析逻辑
uint32_t resolve_path(const char *path) {
    if (path == NULL || *path == '\0') return 0;
    
    uint32_t current_inode = 2; // 根目录
    char name[32];

    // 处理根目录情况
    if (strcmp(path, "/") == 0) return 2;
    if (*path == '/') path++;

    while (*path != '\0') {
        int i = 0;
        // 切分片段
        while (*path != '/' && *path != '\0' && i < 31) {
            name[i++] = *path++;
        }
        name[i] = '\0';
        if (*path == '/') path++;

        // 下钻
        current_inode = find_inode_in_dir(current_inode, name);
        if (current_inode == 0) return 0;
    }
    return current_inode;
}
