#include "filesystem.h"
#include "../kernel/utils/string.h"
#include "../kernel/drivers/serial.h"

/* 
 * Disk Layout:
 * Block 1: Superblock (Magic, Total Blocks, Free Blocks)
 * Block 2-3: Allocation Bitmap (Tracks up to 16,384 blocks)
 * Block 4: Root Directory
 */

static void fs_format(void) {
    serial_puts(SERIAL_COM1, "FS: Formatting empty disk...\n");

    /* 1. Setup Superblock */
    block_t *sb = block_get(1);
    memset(sb->data, 0, BLOCK_DATA_SIZE);
    sb->header.type = FS_MAGIC;
    /* Store total blocks (10MB disk / 1KB block = ~10,000 blocks) */
    *(u32*)(sb->data) = 10000; 
    block_update();

    /* 2. Setup Bitmap (Mark blocks 0-4 as USED) */
    block_t *bitmap = block_get(2);
    memset(bitmap->data, 0, BLOCK_DATA_SIZE);
    bitmap->data[0] = 0x1F; /* Binary 0001 1111 (Blocks 0,1,2,3,4 used) */
    block_update();

    block_flush();
    serial_puts(SERIAL_COM1, "FS: Format complete.\n");
}

void fs_init(void) {
    block_t *sb = block_get(1);
    if (sb->header.type != FS_MAGIC) {
        fs_format();
    } else {
        serial_puts(SERIAL_COM1, "FS: Valid Superblock found.\n");
    }
}

u32 fs_alloc_block(void) {
    block_t *bitmap = block_get(2);
    /* Search for a free bit (0) */
    for (int byte = 0; byte < BLOCK_DATA_SIZE; byte++) {
        if (bitmap->data[byte] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!(bitmap->data[byte] & (1 << bit))) {
                    /* Found a free block! Mark it used. */
                    bitmap->data[byte] |= (1 << bit);
                    block_update();
                    return (byte * 8) + bit;
                }
            }
        }
    }
    kernel_panic("FS: Disk Full!");
    return 0;
}