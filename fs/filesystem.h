#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "block_device.h"

#define FS_MAGIC 0x48545246 /* "FRTH" */

void fs_init(void);
u32  fs_alloc_block(void);
void fs_free_block(u32 block_num);

#endif