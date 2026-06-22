#include <stdint.h>
#include "../include/ext2.h"
#include "../include/debug.h"
#include "../include/fs.h"
#include "../include/disk.h"
#include "../include/fs_runtime.h"
#include "../include/string.h"

// 在指定目录 dir_inode_num 下，寻找名字为 name 的子项，返回它的 Inode 号
uint32_t ext2_lookup(uint32_t dir_inode_num, const char *name) {
    dbg_kv("lookup", "dir_inode", dir_inode_num);
    serial_puts("[lookup] name=");
    serial_puts(name);
    serial_puts("\n");
    struct ext2_inode inode;
    read_inode(dir_inode_num, g_sb, fs_gdt, &inode);

    // 安全检查：必须是个目录
    if ((inode.i_mode & 0xF000) != 0x4000) return 0;

    __attribute__((aligned(8))) static uint8_t dir_block_buf[4096];
    // 读取目录的第一个数据块
    read_fs_block(inode.i_block[0], dir_block_buf); 

    uint32_t offset = 0;
    while (offset < g_block_size) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(dir_block_buf + offset);

        if (entry->rec_len == 0) break;

        if (entry->inode != 0 && entry->name_len > 0) {
           int len = entry->name_len;
            // 提取目录项的名字
            char name_buf[256];
            for (int i = 0; i < len; i++) {
                name_buf[i] = (char)entry->name[i];
            }
            name_buf[len] = '\0';

            // 【核心匹配】如果名字对上了！
            if (strcmp(name_buf, name) == 0) {
                dbg_kv("lookup", "match_inode", entry->inode);
                return entry->inode; // 找到了，返回它的 Inode 号
            }
        }
        offset += entry->rec_len;
    }
    dbg_msg("lookup", "not found");
    return 0; // 没找到
}
