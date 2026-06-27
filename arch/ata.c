#include <stdint.h>
#include <stdbool.h>
#include "../include/ata.h"
#include "../include/debug.h"
#include "../include/io.h"
#include "../include/panic.h"

bool ata_init(void) {
    dbg_msg("ata", "initializing controller");

    // 1. 发送软件复位信号
    outb(ATA_PRIMARY_CTRL, 0x04);
    for (volatile int i = 0; i < 10000; i++); // 延迟等待硬件处理
    outb(ATA_PRIMARY_CTRL, 0x00);
    
    // 2. 复位后必须给控制器时间响应
    // 某些硬件在复位后会忙碌一段时间，需要轮询 BSY 位
    uint32_t timeout = 100000;
    while ((inb(ATA_REG_STATUS) & ATA_SR_BSY) && timeout--);

    if (timeout == 0) {
        dbg_msg("ata", "initialization timed out");
        return false;
    }

    // 3. 探测设备 (选择主盘，使用 LBA 模式)
    outb(ATA_REG_DEVICE, ATA_MASTER);
    for (volatile int i = 0; i < 10000; i++); // 等待设备选择稳定

    // 4. 轮询 DRDY，给设备足够时间就绪
    uint32_t poll = 100000;
    uint8_t status;
    do {
        status = inb(ATA_REG_STATUS);
        if (status == 0xFF) {
            dbg_msg("ata", "no device detected (bus floating)");
            return false;
        }
        if (status & ATA_SR_ERR) {
            dbg_msg("ata", "device reported error during init");
            return false;
        }
    } while (!(status & ATA_SR_DRDY) && poll--);

    if (poll == 0) {
        dbg_msg("ata", "drive not ready (DRDY timeout)");
        return false;
    }

    dbg_msg("ata", "device detected and ready");
    return true;
}
