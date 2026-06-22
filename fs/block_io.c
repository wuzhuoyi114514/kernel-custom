#include <stdint.h>

extern uint32_t g_block_size;
extern void disk_read(uint32_t lba, uint8_t *buffer, uint32_t sector_count);

#define SECTOR_SIZE 512

void read_block(uint32_t block_id, uint8_t *buffer)
{
    if (g_block_size == 0) {
        // FS corrupted
        return;
    }

    uint32_t sectors_per_block = g_block_size / SECTOR_SIZE;

    if (sectors_per_block == 0) {
        // invalid block size
        return;
    }

    uint32_t start_lba = block_id * sectors_per_block;

    disk_read(start_lba, buffer, sectors_per_block);
}

