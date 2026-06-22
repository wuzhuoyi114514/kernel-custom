#include <stdint.h>
#include "../include/ext2.h"

extern void serial_puts(const char *s);
extern void print_hex(uint32_t val);

extern uint32_t g_block_size;
extern uint32_t g_inode_size;
extern void read_fs_block(uint32_t block_id, uint8_t *buffer);

// inode buffer（最大 block size 4K）
__attribute__((aligned(8)))
static uint8_t safe_inode_buf[4096];

void read_inode(uint32_t inode_num,
                struct ext2_superblock *sb,
                struct ext2_group_desc *gdt,
                struct ext2_inode *out_inode)
{
    if (inode_num == 0 || inode_num > sb->s_inodes_count) {
        serial_puts("FS: invalid inode number\n");
        return;
    }

    // 1. inode index（从 0 开始）
    uint32_t local_inode_idx = inode_num - 1;

    // 2. 使用 superblock inode size（关键修复点）
    uint32_t inode_size = g_inode_size;

    uint32_t inodes_per_block = g_block_size / inode_size;

    if (inodes_per_block == 0) {
        serial_puts("FS: inodes_per_block = 0\n");
        while (1);
    }

    // 3. 直接使用 struct（修复 GDT 手动解析 bug）
    uint32_t inode_table_block = gdt->bg_inode_table;

    // 4. inode 所在 block
    uint32_t block_num =
        inode_table_block + (local_inode_idx / inodes_per_block);

    // 5. 偏移
    uint32_t offset =
        (local_inode_idx % inodes_per_block) * inode_size;

    // 6. 读取 inode 所在 block
    read_fs_block(block_num, safe_inode_buf);

    // 7. 拷贝 inode
    uint8_t *src = safe_inode_buf + offset;
    uint8_t *dst = (uint8_t *)out_inode;

    for (uint32_t i = 0; i < inode_size; i++) {
        dst[i] = src[i];
    }

    // debug
    serial_puts("FS: inode loaded OK\n");
}
