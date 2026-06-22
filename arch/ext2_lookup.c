#include <stdint.h>
#include "../include/ext2.h"
#include "../include/fs.h"
#include "../include/disk.h"

extern struct ext2_group_desc *fs_gdt;
extern uint32_t g_block_size;
extern void read_inode(uint32_t inode_num, struct ext2_group_desc *gdt, struct ext2_inode *inode);

// 外部实现的通用字符串对比
extern int strcmp(const char *a, const char *b); 

// 在指定目录 dir_inode_num 下，寻找名字为 name 的子项，返回它的 Inode 号
uint32_t ext2_lookup(uint32_t dir_inode_num, const char *name) {
    struct ext2_inode inode;
    read_inode(dir_inode_num, fs_gdt, &inode);

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
                return entry->inode; // 找到了，返回它的 Inode 号
            }
        }
        offset += entry->rec_len;
    }
    return 0; // 没找到
}
