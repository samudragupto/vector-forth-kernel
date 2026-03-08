#ifndef STACK_H
#define STACK_H

#include "../../kernel/core/kernel.h"

#define DATA_STACK_SIZE 256
#define RETURN_STACK_SIZE 256

/*--- Data Stack ---*/
void ds_push(i64 value);
i64  ds_pop(void);
i64  ds_peek(void);
i64  ds_pick(int n);
i64  ds_depth(void);
void ds_clear(void);
void ds_print(void);

/*--- Return Stack ---*/
void rs_push(u64 value);
u64  rs_pop(void);
u64  rs_peek(void);
u64  rs_depth(void);
void rs_clear(void);

#endif