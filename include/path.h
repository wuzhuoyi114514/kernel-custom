#ifndef PATH_H
#define PATH_H

#include <stdint.h>

// 输入路径字符串，返回对应的 Inode 号，返回 0 表示找不到
uint32_t resolve_path(const char *path);

#endif
