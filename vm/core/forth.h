#ifndef FORTH_H
#define FORTH_H

#include "../../kernel/core/kernel.h"

/* State of the interpreter */
#define STATE_INTERPRET 0
#define STATE_COMPILE   1

/* Initialize Forth VM and register all primitives */
void forth_init(void);

/* Start the interactive Forth REPL */
void forth_run(void);

/* Interpret a single line of input */
void forth_eval(const char *line);

/* Get/Set interpreter state */
int  forth_get_state(void);
void forth_set_state(int state);

/* Get/Set numeric base */
int  forth_get_base(void);
void forth_set_base(int base);

#endif