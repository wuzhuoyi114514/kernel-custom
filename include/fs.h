#ifndef FS_H
#define FS_H

#include <stdint.h>
#include "disk.h"

void read_fs_block(uint32_t block_id, uint8_t *buffer);

#endif
