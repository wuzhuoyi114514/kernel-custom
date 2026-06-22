#include <stdint.h>
#include "../include/ata.h"
#include "../include/debug.h"

extern void panic(const char *msg);
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);
extern void insw(uint16_t port, void *addr, uint32_t count);

bool ata_init(void) {
    serial_puts("ATA: Initializing...\n");

    // 1. 发送软件复位信号
    outb(ATA_PRIMARY_CTRL, 0x04);
    for (volatile int i = 0; i < 10000; i++); // 延迟等待硬件处理
    outb(ATA_PRIMARY_CTRL, 0x00);
    
    // 2. 复位后必须给控制器时间响应
    // 某些硬件在复位后会忙碌一段时间，需要轮询 BSY 位
    uint32_t timeout = 100000;
    while ((inb(ATA_REG_STATUS) & ATA_SR_BSY) && timeout--);

    if (timeout == 0) {
        serial_puts("ATA: Initialization timed out (Controller stuck).\n");
        return false;
    }

    // 3. 探测设备 (选择主盘 0xA0)
    outb(ATA_REG_DEVICE, 0xA0);
    for (volatile int i = 0; i < 1000; i++); // 等待设备选择稳定

    uint8_t status = inb(ATA_REG_STATUS);
    if (status == 0xFF) {
        serial_puts("ATA: No device detected (Bus floating).\n");
        return false;
    }
    // 如果没有盘，DRDY 永远不会变高
    if (status == 0xFF || !(status & 0x40)) {
        serial_puts("ATA: No device detected (Bus floating or drive not ready).\n");
        return false; 
    }

    // 如果状态寄存器显示有错误位 (ERR, 位 0)
    if (status & 0x01) {
        serial_puts("ATA: Device reported error during init.\n");
        return false;
    }

    serial_puts("ATA: Device detected and ready.\n");
    return true;
}
