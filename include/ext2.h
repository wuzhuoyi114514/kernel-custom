#ifndef EXT2_H
#define EXT2_H
#define EXT2_S_IFMT     0xF000  // 文件类型掩码
#define EXT2_S_IFDIR    0x4000  // 目录类型

#include <stdint.h>

struct ext2_superblock {
    uint32_t s_inodes_count;      /* Inodes count */
    uint32_t s_blocks_count;      /* Blocks count */
    uint32_t s_r_blocks_count;    /* Reserved blocks count */
    uint32_t s_free_blocks_count; /* Free blocks count */
    uint32_t s_free_inodes_count; /* Free inodes count */
    uint32_t s_first_data_block;  /* First Data Block */
    uint32_t s_log_block_size;    /* Block size */
    int32_t  s_log_frag_size;     /* Fragment size */
    uint32_t s_blocks_per_group;  /* # Blocks per group */
    uint32_t s_frags_per_group;   /* # Fragments per group */
    uint32_t s_inodes_per_group;  /* # Inodes per group */
    uint32_t s_mtime;             /* Mount time */
    uint32_t s_wtime;             /* Write time */
    uint16_t s_mnt_count;         /* Mount count */
    uint16_t s_max_mnt_count;     /* Maximal mount count */
    uint16_t s_magic;             /* Magic signature */
    uint16_t s_state;             /* File system state */
    uint16_t s_errors;            /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;   /* minor revision level */
    uint32_t s_lastcheck;         /* time of last check */
    uint32_t s_checkinterval;     /* max time between checks */
    uint32_t s_creator_os;        /* OS */
    uint32_t s_rev_level;         /* Revision level */
    uint16_t s_def_resuid;        /* Default uid for reserved blocks */
    uint16_t s_def_resgid;        /* Default gid for reserved blocks */
    
    /* 这些是必须补充的字段 */
    uint32_t s_first_ino;         /* First non-reserved inode */
    uint16_t s_inode_size;        /* size of inode structure */
    uint16_t s_block_group_nr;    /* block group # of this superblock */
    uint32_t s_feature_compat;    /* compatible feature set */
    uint32_t s_feature_incompat;  /* incompatible feature set */
    uint32_t s_feature_ro_compat; /* readonly-compatible feature set */
    uint8_t  s_uuid[16];          /* 128-bit uuid for volume */
    char     s_volume_name[16];   /* volume name */
} __attribute__((packed));

// 组描述符：描述每个块组的元数据位置
struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;       // 非常重要：指向 Inode 表的块号
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

struct ext2_dir_entry_2 {
    uint32_t inode;     // 对应的 Inode 号（如果为 0 表示该项已删除）
    uint16_t rec_len;   // 当前目录项的总长度（用于寻找下一个项的偏移）
    uint8_t  name_len;  // 文件名的实际长度
    uint8_t  file_type; // 文件类型（1: 普通文件, 2: 目录, 7: 符号链接等）
    char     name[];    // 变长文件名（注意：磁盘上没有 '\0' 结尾！）
}__attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;        // 文件类型和权限
    uint16_t i_uid;         // 用户 ID
    uint32_t i_size;        // 文件大小 (字节)
    uint32_t i_atime;       // 最后访问时间
    uint32_t i_ctime;       // 创建时间
    uint32_t i_mtime;       // 最后修改时间
    uint32_t i_dtime;       // 删除时间
    uint16_t i_gid;         // 组 ID
    uint16_t i_links_count; // 硬链接数
    uint32_t i_blocks;      // 占用的扇区数
    uint32_t i_flags;       // 文件标志
    uint32_t i_osd1;        // OS 特有值
    uint32_t i_block[15];   // !!核心!! 15 个块指针 (前12个直接指向数据)
    uint32_t i_generation;  // 版本号 (用于 NFS)
    uint32_t i_file_acl;    // 文件 ACL
    uint32_t i_dir_acl;     // 目录 ACL
    uint32_t i_faddr;       // 分段地址
    uint8_t  i_frag;        // 片段编号
    uint8_t  i_fsize;       // 片段大小
    uint16_t i_pad1;
    uint32_t i_reserved2[2];
} __attribute__((packed));
struct ext2_group_desc; // 前置声明
struct ext2_inode;      // 前置声明
                        
void read_inode(uint32_t inode_num,  struct ext2_superblock *sb, struct ext2_group_desc *gdt, struct ext2_inode *out_inode);
#endif
