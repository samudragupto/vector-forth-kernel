#include "block_device.h"
#include "../kernel/drivers/ata.h"
#include "../kernel/utils/string.h"
#include "../kernel/drivers/serial.h"

typedef struct {
    u32     block_num;
    u8      dirty;
    u8      valid;
    u64     last_used;
    block_t block;
} block_buffer_t;

static block_buffer_t buffers[NUM_BUFFERS];
static u32 most_recent_buffer = 0;
extern volatile u64 kernel_ticks;

/* Simple CRC32 Implementation */
u32 calculate_crc32(const u8 *data, size_t length) {
    u32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

void block_init(void) {
    memset(buffers, 0, sizeof(buffers));
    ata_init();
    serial_puts(SERIAL_COM1, "FS: Block cache initialized\n");
}

static void write_buffer_to_disk(int idx) {
    if (!buffers[idx].valid || !buffers[idx].dirty) return;
    
    /* Calculate CRC32 of the 1008-byte data field before saving */
    buffers[idx].block.header.crc = calculate_crc32(buffers[idx].block.data, BLOCK_DATA_SIZE);

    u32 start_sector = DISK_BLOCK_OFFSET_SECTORS + (buffers[idx].block_num * 2);
    ata_write_sector(start_sector, (u8 *)&buffers[idx].block);
    ata_write_sector(start_sector + 1, ((u8 *)&buffers[idx].block) + 512);
    
    buffers[idx].dirty = 0;
}

block_t* block_get(u32 block_num) {
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (buffers[i].valid && buffers[i].block_num == block_num) {
            buffers[i].last_used = kernel_ticks;
            most_recent_buffer = i;
            return &buffers[i].block;
        }
    }

    int oldest_idx = 0;
    u64 oldest_time = 0xFFFFFFFFFFFFFFFFULL;
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (!buffers[i].valid) { oldest_idx = i; break; }
        if (buffers[i].last_used < oldest_time) {
            oldest_time = buffers[i].last_used;
            oldest_idx = i;
        }
    }

    write_buffer_to_disk(oldest_idx);

    u32 start_sector = DISK_BLOCK_OFFSET_SECTORS + (block_num * 2);
    ata_read_sector(start_sector, (u8 *)&buffers[oldest_idx].block);
    ata_read_sector(start_sector + 1, ((u8 *)&buffers[oldest_idx].block) + 512);

    /* Verify CRC (Warn if mismatch, but don't panic on empty blocks) */
    u32 expected_crc = calculate_crc32(buffers[oldest_idx].block.data, BLOCK_DATA_SIZE);
    if (buffers[oldest_idx].block.header.crc != 0 && 
        buffers[oldest_idx].block.header.crc != expected_crc) {
        serial_puts(SERIAL_COM1, "FS WARNING: CRC mismatch on block load!\n");
    }

    buffers[oldest_idx].block_num = block_num;
    buffers[oldest_idx].valid = 1;
    buffers[oldest_idx].dirty = 0;
    buffers[oldest_idx].last_used = kernel_ticks;
    
    most_recent_buffer = oldest_idx;
    return &buffers[oldest_idx].block;
}

void block_update(void) {
    if (buffers[most_recent_buffer].valid) buffers[most_recent_buffer].dirty = 1;
}

void block_flush(void) {
    for (int i = 0; i < NUM_BUFFERS; i++) write_buffer_to_disk(i);
}