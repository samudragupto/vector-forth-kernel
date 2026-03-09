#include "block_device.h"
#include "../kernel/drivers/ata.h"
#include "../kernel/utils/string.h"
#include "../kernel/drivers/serial.h"

typedef struct {
    u32 block_num;
    u8  dirty;
    u8  valid;
    u64 last_used;
    u8  data[BLOCK_SIZE];
} block_buffer_t;

static block_buffer_t buffers[NUM_BUFFERS];
static u32 most_recent_buffer = 0;
extern volatile u64 kernel_ticks;

void block_init(void) {
    memset(buffers, 0, sizeof(buffers));
    ata_init();
    serial_puts(SERIAL_COM1, "FS: Block cache initialized (8 buffers)\n");
}

/* Write a dirty buffer back to the physical disk */
static void write_buffer_to_disk(int idx) {
    if (!buffers[idx].valid || !buffers[idx].dirty) return;
    
    /* 1 Block = 1024 bytes = 2 physical sectors */
    u32 start_sector = DISK_BLOCK_OFFSET_SECTORS + (buffers[idx].block_num * 2);
    
    ata_write_sector(start_sector, buffers[idx].data);
    ata_write_sector(start_sector + 1, buffers[idx].data + 512);
    
    buffers[idx].dirty = 0;
}

u8* block_get(u32 block_num) {
    /* 1. Check if it's already in RAM (Cache Hit) */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (buffers[i].valid && buffers[i].block_num == block_num) {
            buffers[i].last_used = kernel_ticks;
            most_recent_buffer = i;
            return buffers[i].data;
        }
    }

    /* 2. Cache Miss! Find the oldest buffer to evict */
    int oldest_idx = 0;
    u64 oldest_time = 0xFFFFFFFFFFFFFFFFULL;
    
    for (int i = 0; i < NUM_BUFFERS; i++) {
        if (!buffers[i].valid) {
            oldest_idx = i;
            break;
        }
        if (buffers[i].last_used < oldest_time) {
            oldest_time = buffers[i].last_used;
            oldest_idx = i;
        }
    }

    /* 3. If the oldest buffer has unsaved changes, save it to disk first */
    write_buffer_to_disk(oldest_idx);

    /* 4. Load the requested block from disk into the buffer */
    u32 start_sector = DISK_BLOCK_OFFSET_SECTORS + (block_num * 2);
    ata_read_sector(start_sector, buffers[oldest_idx].data);
    ata_read_sector(start_sector + 1, buffers[oldest_idx].data + 512);

    buffers[oldest_idx].block_num = block_num;
    buffers[oldest_idx].valid = 1;
    buffers[oldest_idx].dirty = 0;
    buffers[oldest_idx].last_used = kernel_ticks;
    
    most_recent_buffer = oldest_idx;
    return buffers[oldest_idx].data;
}

/* Flag the most recently accessed block as modified */
void block_update(void) {
    if (buffers[most_recent_buffer].valid) {
        buffers[most_recent_buffer].dirty = 1;
    }
}

/* Force all modified blocks to be saved to disk */
void block_flush(void) {
    for (int i = 0; i < NUM_BUFFERS; i++) {
        write_buffer_to_disk(i);
    }
    serial_puts(SERIAL_COM1, "FS: All dirty buffers flushed to disk.\n");
}