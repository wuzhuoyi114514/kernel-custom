#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* =========================
 * ATA I/O Ports (Primary Bus)
 * ========================= */
#define ATA_PRIMARY_IO       0x1F0
#define ATA_PRIMARY_CTRL     0x3F6

#define ATA_REG_DATA         0x1F0
#define ATA_REG_ERROR        0x1F1
#define ATA_REG_SECCOUNT     0x1F2
#define ATA_REG_LBA_LOW      0x1F3
#define ATA_REG_LBA_MID      0x1F4
#define ATA_REG_LBA_HIGH     0x1F5
#define ATA_REG_DEVICE       0x1F6
#define ATA_REG_STATUS       0x1F7
#define ATA_REG_COMMAND      0x1F7
#define ATA_REG_ALTSTATUS    0x3F6

/* =========================
 * ATA Status Flags
 * ========================= */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01
#define ATA_SR_DRDY 0x40

/* =========================
 * Device Selection
 * ========================= */
#define ATA_MASTER 0xE0
#define ATA_SLAVE  0xF0

/* =========================
 * API
 * ========================= */

/**
 * 初始化 ATA 控制器（必须在任何 read 之前调用）
 */
bool ata_init(void);

/**
 * 读取扇区（PIO mode）
 * @param lba 起始 LBA
 * @param buffer 输出缓冲区（必须 >= sector_count * 512）
 * @param sector_count 扇区数量
 */
void disk_read(uint32_t lba, uint8_t *buffer, uint32_t sector_count);

#endif
