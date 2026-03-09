#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "../kernel/core/kernel.h"

#define BLOCK_SIZE      1024
#define BLOCK_DATA_SIZE 1008
#define NUM_BUFFERS     8

#define DISK_BLOCK_OFFSET_SECTORS 1024

/* 16-Byte Header matching Diagram 7 */
typedef struct {
    u32 type;       /* Block type (e.g., Data, Superblock) */
    u32 prev;       /* Previous block in chain */
    u32 next;       /* Next block in chain */
    u32 crc;        /* CRC32 Checksum of data */
} block_header_t;

/* Full 1024-byte Block */
typedef struct {
    block_header_t header;
    u8             data[BLOCK_DATA_SIZE];
} block_t;

void     block_init(void);
block_t *block_get(u32 block_num);
void     block_update(void);
void     block_flush(void);
u32      calculate_crc32(const u8 *data, size_t length);

#endif