#ifndef BLOCK_DEVICE_H
#define BLOCK_DEVICE_H

#include "../kernel/core/kernel.h"

#define BLOCK_SIZE      1024
#define NUM_BUFFERS     8

/* To protect the kernel, Block 0 maps to physical sector 1024 (Offset 512KB on disk) */
#define DISK_BLOCK_OFFSET_SECTORS 1024

void block_init(void);
u8*  block_get(u32 block_num);
void block_update(void);
void block_flush(void);

#endif