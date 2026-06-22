#include <stdint.h>
#include "../include/ext2.h"
#include "../include/debug.h"
#include "../include/disk.h"
#include "../include/fs_runtime.h"

uint32_t g_block_size = 1024;
uint32_t g_inode_size = 128;
uint32_t g_inodes_per_group = 0;
uint32_t g_inodes_per_block = 0;

// ==================== 【核心新增 1】: MBR 结构体与全局变量 ====================
uint32_t ext2_start_lba = 0; // 全局变量：Ext2 分区在磁盘上的起始绝对 LBA

struct mbr_partition {
    uint8_t  boot_indicator;   
    uint8_t  start_head;       
    uint8_t  start_sector;     
    uint8_t  start_cylinder;   
    uint8_t  partition_type;   // 分区类型 (Linux Ext 系列通常是 0x83)
    uint8_t  end_head;
    uint8_t  end_sector;
    uint8_t  end_cylinder;
    uint32_t start_lba;        // 关键：该分区的绝对起始 LBA
    uint32_t total_sectors;    
} __attribute__((packed));

struct mbr_sector {
    uint8_t             boot_code[446];       
    struct mbr_partition partitions[4];        
    uint16_t            signature;            // 0xAA55
} __attribute__((packed));

 

// ==================== 【核心新增 2】: 动态探测分区函数 ====================
void probe_ext2_partition(void) {
    dbg_msg("fs", "probing ext2 partition");
    uint8_t mbr_buffer[512];
    
    // 读取磁盘第一个物理扇区 (LBA 0)
    disk_read(0, mbr_buffer, 1);
    struct mbr_sector *mbr = (struct mbr_sector *)mbr_buffer;

    // 如果 MBR 签名有效，就去遍历分区表
    if (mbr->signature == 0xAA55) {
        for (int i = 0; i < 4; i++) {
            if (mbr->partitions[i].partition_type == 0x83) {
                ext2_start_lba = mbr->partitions[i].start_lba;
                dbg_kv("fs", "ext2_start_lba", ext2_start_lba);
                return;
            }
        }
    }

    // 降级处理：没找到或者没分区表，说明是裸镜像，起始 LBA 保持为 0
    ext2_start_lba = 0;
    dbg_msg("fs", "no partition table, defaulting to raw lba 0");
}

/**
 * 初始化块大小 (从超级块读取)
 */
void fs_init(struct ext2_superblock *sb) {
    dbg_msg("fs", "initializing ext2 metadata");
    // Ext2 块大小计算公式: 1024 << s_log_block_size
    g_block_size = 1024 << sb->s_log_block_size;

    if (sb->s_rev_level >= 1) {
        g_inode_size = sb->s_inode_size;
    } else {
        g_inode_size = 128;
    }
    
    // 3. 预计算每个块能存多少个 Inode，这样读取时就不需要实时做除法了
    g_inodes_per_block = g_block_size / g_inode_size;
    
    dbg_kv("fs", "block_size", g_block_size);
    dbg_kv("fs", "inode_size", g_inode_size);
    dbg_kv("fs", "inodes_per_block", g_inodes_per_block);
}


/**
 * 块读取接口
 * block_id: 逻辑块号
 * buffer:   目标缓冲区
 */
void read_fs_block(uint32_t block_id, uint8_t *buf)
{
    if (block_id > 0x100000) { 
        dbg_msg("fs", "invalid block id");
        dbg_hex("fs", block_id);
        while(1); 
    }
    uint32_t sectors = g_block_size / 512;
    uint32_t lba = block_id * sectors;
    dbg_kv("fs", "block_id", block_id);
    dbg_kv("fs", "sectors_per_block", sectors);
    dbg_kv("fs", "absolute_lba", ext2_start_lba + lba);

    // ==================== 【核心修改 3】：全面解开硬编码 ====================
    // 不管 Ext2 内部怎么算 LBA，丢给底层磁盘驱动时，强制叠加上分区的起始绝对 LBA！
    disk_read(ext2_start_lba + lba, buf, sectors);
}
