#include <stdint.h>
#include "../include/debug.h"
#include "../include/io.h"

#define ATA_BASE 0x1F0
#define ATA_REG_DATA        (ATA_BASE + 0)
#define ATA_REG_SECCOUNT    (ATA_BASE + 2)
#define ATA_REG_LBA_LOW     (ATA_BASE + 3)
#define ATA_REG_LBA_MID     (ATA_BASE + 4)
#define ATA_REG_LBA_HIGH    (ATA_BASE + 5)
#define ATA_REG_DEVICE      (ATA_BASE + 6)
#define ATA_REG_COMMAND     (ATA_BASE + 7)
#define ATA_REG_STATUS      (ATA_BASE + 7)
#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);
extern void insw(uint16_t port, void *addr, uint32_t count);

void disk_read_ex(uint32_t drive, uint32_t lba, uint8_t *buffer, uint32_t sector_count) {
    while (inb(ATA_REG_STATUS) & ATA_SR_BSY);

    uint8_t devsel = (drive == 0) ? 0xE0 : 0xF0;
    outb(ATA_REG_DEVICE, devsel | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, sector_count);
    outb(ATA_REG_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_REG_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_REG_COMMAND, 0x20);

    for (volatile int d = 0; d < 1000; d++);

    for (uint32_t i = 0; i < sector_count; i++) {
        int error_count = 0;
        uint8_t status;

        while (error_count < 3) {
            int timeout = 1000000;
            uint8_t *sector_buffer = buffer + (i * 512);

            while (timeout--) {
                status = inb(0x3F6);

                if (status & ATA_SR_ERR) {
                    dbg_msg("disk", "ata read error (retry...)");
                    break;
                }

                if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                    break;
            }

            if (timeout > 0 && !(status & ATA_SR_ERR)) {
                insw(ATA_REG_DATA, sector_buffer, 256);
                break;
            }

            error_count++;
            for (volatile int k = 0; k < 10000; k++);
        }

        if (error_count >= 3) {
            dbg_msg("disk", "ata timeout after 3 retries");
            while(1);
        }
    }
}

void disk_read(uint32_t lba, uint8_t *buffer, uint32_t sector_count) {
    disk_read_ex(0, lba, buffer, sector_count);
}
