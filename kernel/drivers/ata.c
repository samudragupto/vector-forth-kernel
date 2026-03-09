#include "ata.h"
#include "serial.h"

static void ata_wait_bsy(void) {
    while (inb(ATA_PRIMARY_COMM_STAT) & ATA_SR_BSY) {
        /* Wait for drive to not be busy */
    }
}

static void ata_wait_drq(void) {
    while (!(inb(ATA_PRIMARY_COMM_STAT) & ATA_SR_DRQ)) {
        /* Wait for drive to be ready to transfer data */
        if (inb(ATA_PRIMARY_COMM_STAT) & ATA_SR_ERR) {
            kernel_panic("ATA Disk Error!");
        }
    }
}

void ata_init(void) {
    serial_puts(SERIAL_COM1, "ATA: Initializing PIO driver...\n");
    /* Simple initialization: just check if the bus is floating */
    if (inb(ATA_PRIMARY_COMM_STAT) == 0xFF) {
        serial_puts(SERIAL_COM1, "ATA: No drive detected on Primary bus.\n");
    } else {
        serial_puts(SERIAL_COM1, "ATA: Drive detected.\n");
    }
}

int ata_read_sector(u32 lba, u8 *buffer) {
    ata_wait_bsy();
    
    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (u8) lba);
    outb(ATA_PRIMARY_LBA_MID, (u8)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (u8)(lba >> 16));
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_READ_PIO);

    ata_wait_bsy();
    ata_wait_drq();

    /* Read 256 16-bit words (512 bytes) */
    u16 *ptr = (u16 *)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_PRIMARY_DATA);
    }
    
    /* 400ns delay for older drives */
    for (int i = 0; i < 4; i++) inb(ATA_PRIMARY_COMM_STAT);
    return 0;
}

int ata_write_sector(u32 lba, u8 *buffer) {
    ata_wait_bsy();

    outb(ATA_PRIMARY_DRV_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (u8) lba);
    outb(ATA_PRIMARY_LBA_MID, (u8)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (u8)(lba >> 16));
    outb(ATA_PRIMARY_COMM_STAT, ATA_CMD_WRITE_PIO);

    ata_wait_bsy();
    ata_wait_drq();

    /* Write 256 16-bit words (512 bytes) */
    u16 *ptr = (u16 *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_DATA, ptr[i]);
    }

    /* Cache flush */
    outb(ATA_PRIMARY_COMM_STAT, 0xE7);
    ata_wait_bsy();
    return 0;
}