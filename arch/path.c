#include "path.h"
#include "ext2.h"
#include "string.h"
#include "fs.h"
#include "fs_runtime.h"
#include "shell_state.h"

// 查找特定目录下的条目 (你之前的逻辑)
uint32_t find_inode_in_dir(uint32_t parent_inode, const char *name) {
    struct ext2_inode dir_inode;
   read_inode(parent_inode, g_sb, fs_gdt, &dir_inode);

    // 假设目录大小在 i_block[0] (简单处理，没处理间接块)
    uint8_t block_buf[1024]; 
    read_fs_block(dir_inode.i_block[0], block_buf);

    uint32_t offset = 0;
    while (offset < 1024) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(block_buf + offset);
        if (entry->inode == 0) break;

        // 比较名字
        if (strncmp(entry->name, name, entry->name_len) == 0 && entry->name_len == strlen(name)) {
            return entry->inode;
        }
        offset += entry->rec_len;
    }
    return 0;
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
