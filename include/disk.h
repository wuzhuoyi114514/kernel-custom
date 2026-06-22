#ifndef DISK_H
#define DISK_H

#include <stdint.h>

void disk_read(uint32_t lba, uint8_t *buffer, uint32_t sector_count);

#endif
