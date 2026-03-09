#ifndef ATA_H
#define ATA_H

#include "../core/kernel.h"

/* Primary ATA bus I/O ports */
#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRV_HEAD     0x1F6
#define ATA_PRIMARY_COMM_STAT    0x1F7

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30

#define ATA_SR_BSY               0x80    /* Busy */
#define ATA_SR_DRQ               0x08    /* Data request ready */
#define ATA_SR_ERR               0x01    /* Error */

void ata_init(void);
int  ata_read_sector(u32 lba, u8 *buffer);
int  ata_write_sector(u32 lba, u8 *buffer);

#endif