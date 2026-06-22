#ifndef FS_RUNTIME_H
#define FS_RUNTIME_H

#include <stdint.h>
#include "ext2.h"

extern uint32_t g_block_size;
extern uint32_t g_inode_size;
extern uint32_t g_inodes_per_group;
extern uint32_t g_inodes_per_block;
extern uint32_t ext2_start_lba;
extern struct ext2_group_desc *fs_gdt;
extern struct ext2_superblock *g_sb;

void probe_ext2_partition(void);
void fs_init(struct ext2_superblock *sb);
void read_fs_block(uint32_t block_id, uint8_t *buffer);

#endif
