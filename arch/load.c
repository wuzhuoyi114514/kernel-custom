#include "../include/ext2.h"
#include <stdint.h>
#include "../include/debug.h"

// 假设这些宏或变量在你的系统中可用
extern uint32_t g_block_size; 
extern void read_fs_block(uint32_t block_id, uint8_t *buffer);
extern void read_inode(uint32_t inode_num, struct ext2_group_desc *gdt, struct ext2_inode *out_inode);
extern struct ext2_group_desc *fs_gdt;

bool load_file_to_memory(uint32_t inode_num, uint8_t *dest_addr) {
    struct ext2_inode inode;
    read_inode(inode_num, fs_gdt, &inode);

    // 1. 先计算块总数，确保变量被正确定义
    uint32_t total_blocks = (inode.i_size + g_block_size - 1) / g_block_size;

    if (total_blocks > 12) {
    serial_puts("Error: File too large, indirect blocks not implemented yet!\n");
    return false;
}
    // 检查是否为空文件
    if (inode.i_size == 0) return false;

    uint8_t *current_ptr = dest_addr;

    // 遍历直接块 (i_block[0] 到 i_block[11])
    for (uint32_t i = 0; i < total_blocks && i < 12; i++) {
        if (inode.i_block[i] == 0) continue; // 跳过空洞块

        read_fs_block(inode.i_block[i], current_ptr);
        current_ptr += g_block_size;
    }

    // 注意：如果 total_blocks > 12，这里需要处理间接块 (i_block[12], 13, 14)
    // 对于简易内核，通常前 12 个块（12KB-48KB）已经足够装下一个简单的 ELF 测试程序了
    
    return true;
}
