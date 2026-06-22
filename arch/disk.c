#include <stdint.h>
#include "../include/debug.h"

#define ATA_BASE 0x1F0
#define ATA_REG_DATA        (ATA_BASE + 0) // <-- 这一行就是缺失的
#define ATA_REG_SECCOUNT    (ATA_BASE + 2)
#define ATA_REG_LBA_LOW     (ATA_BASE + 3)
#define ATA_REG_LBA_MID     (ATA_BASE + 4)
#define ATA_REG_LBA_HIGH    (ATA_BASE + 5)
#define ATA_REG_DEVICE      (ATA_BASE + 6)
#define ATA_REG_COMMAND     (ATA_BASE + 7)
#define ATA_REG_STATUS      (ATA_BASE + 7)
#define ATA_SR_BSY  0x80 
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR          0x01

// 外部汇编函数声明
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);
extern void insw(uint16_t port, void *addr, uint32_t count);

void disk_read(uint32_t lba, uint8_t *buffer, uint32_t sector_count) {
    // 1. 等待驱动器不忙
    while (inb(ATA_REG_STATUS) & ATA_SR_BSY);

    // 2. 发送 LBA 参数
    outb(ATA_REG_DEVICE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_REG_SECCOUNT, sector_count);
    outb(ATA_REG_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_REG_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_REG_LBA_HIGH, (uint8_t)(lba >> 16));

    // 3. 发送读取命令
    outb(ATA_REG_COMMAND, 0x20);

    // 4. 关键：等待缓冲区准备好
    for (uint32_t i = 0; i < sector_count; i++) {
        int timeout = 1000000;
        while (timeout--) {
            uint8_t status = inb(ATA_REG_STATUS);
            
            // 如果报错
            if (status & ATA_SR_ERR) {
                serial_puts("ATA READ ERROR\n");
                while(1);
            }
            
            // 只要不忙且有数据请求，即可读取
            if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
                break;
        }

        if (timeout == 0) {
            serial_puts("ATA TIMEOUT\n");
            while(1);
        }

        // 读取 256 个字 (512 字节)
        insw(ATA_REG_DATA, buffer + (i * 512), 256);
    }
}
