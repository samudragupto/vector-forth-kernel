#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../core/kernel.h"

void keyboard_init(void);
char keyboard_getchar(void);
int  keyboard_available(void);

#endif