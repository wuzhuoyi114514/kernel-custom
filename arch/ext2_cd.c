#include <stdint.h>
#include "../include/ext2.h"
#include "../include/vga.h"
#include "../include/fs_runtime.h"
#include "../include/shell_state.h"
#include "../include/string.h"

extern uint32_t ext2_lookup(uint32_t dir_inode_num, const char *name);

void ext2_cd(const char *path) {
    if (path == 0 || path[0] == '\0') return; 

    // 1. 查找目标 Inode
    uint32_t target_inode = ext2_lookup(g_cwd_inode, path);
    if (target_inode == 0) {
        vga_puts("cd: No such directory.\n");
        return;
    }

    // 2. 【核心修改】亲自读取 Inode 属性，拦截非目录文件
    struct ext2_inode target_ino_data;
    read_inode(target_inode, g_sb, fs_gdt, &target_ino_data);
    if ((target_ino_data.i_mode & 0xF000) != 0x4000) {
        vga_puts("cd: Not a directory.\n");
        return;
    }

    // 3. 既然是目录，开始原有的路径字符串维护逻辑
    if (strcmp(path, ".") == 0) {
        // 原地不动
    } 
    else if (strcmp(path, "..") == 0) {
        int len = 0; while (g_cwd_path[len] != '\0') len++;
        int last_slash = -1;
        for (int i = 0; i < len; i++) if (g_cwd_path[i] == '/') last_slash = i;
        if (last_slash > 0) g_cwd_path[last_slash] = '\0';
        else if (last_slash == 0 && len > 1) g_cwd_path[1] = '\0';
    } 
    else {
        int len = 0; while (g_cwd_path[len] != '\0') len++;
        if (len == 1 && g_cwd_path[0] == '/') {
            int i = 0; while (path[i] != '\0') { g_cwd_path[1 + i] = path[i]; i++; }
            g_cwd_path[1 + i] = '\0';
        } else {
            g_cwd_path[len] = '/';
            int i = 0; while (path[i] != '\0') { g_cwd_path[len + 1 + i] = path[i]; i++; }
            g_cwd_path[len + 1 + i] = '\0';
        }
    }

    g_cwd_inode = target_inode; 
}
