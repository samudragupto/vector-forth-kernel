#ifndef FORTH_H
#define FORTH_H

#include "../../kernel/core/kernel.h"

/* Interpreter states */
#define STATE_INTERPRET 0
#define STATE_COMPILE   1

void forth_init(void);
void forth_run(void);
void forth_eval(const char *line);

int  forth_get_state(void);
void forth_set_state(int state);
int  forth_get_base(void);
void forth_set_base(int base);

/*--- Inner Interpreter Code Fields ---*/
void do_colon(void);     /* For compiled words */
void do_literal(void);   /* For numbers */
void do_variable(void);  /* For variables */
void do_constant(void);  /* For constants */

/* Execute a word given its dictionary entry pointer */
void forth_execute(u8 *entry);

#endif