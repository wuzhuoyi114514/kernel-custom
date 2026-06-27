#ifndef SHELL_STATE_H
#define SHELL_STATE_H

#include <stdint.h>

#include "vfs.h"

#define SHELL_CWD_PATH_MAX 128

extern vfs_node_t g_cwd_node;
extern char g_cwd_path[SHELL_CWD_PATH_MAX];

#endif
