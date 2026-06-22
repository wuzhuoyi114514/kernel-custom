#include <stdint.h>
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

    // 3. 探测设备 (选择主盘 0xA0)
    outb(ATA_REG_DEVICE, 0xA0);
    for (volatile int i = 0; i < 1000; i++); // 等待设备选择稳定

    uint8_t status = inb(ATA_REG_STATUS);
    if (status == 0xFF) {
        dbg_msg("ata", "no device detected (bus floating)");
        return false;
    }
    // 如果没有盘，DRDY 永远不会变高
    if (status == 0xFF || !(status & 0x40)) {
        dbg_msg("ata", "no device detected or drive not ready");
        return false; 
    }

    // 如果状态寄存器显示有错误位 (ERR, 位 0)
    if (status & 0x01) {
        dbg_msg("ata", "device reported error during init");
        return false;
    }

    dbg_msg("ata", "device detected and ready");
    return true;
}
