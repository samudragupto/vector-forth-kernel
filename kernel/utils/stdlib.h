#ifndef STDLIB_H
#define STDLIB_H

#include "../core/kernel.h"

char *itoa(int value, char *str, int base);
char *utoa(unsigned int value, char *str, int base);
char *ltoa(long value, char *str, int base);
char *ultoa(unsigned long value, char *str, int base);
long  strtol(const char *str, char **endptr, int base);
unsigned long strtoul(const char *str, char **endptr, int base);
int   atoi(const char *str);
int   abs_val(int x);
int   isdigit(int c);
int   isalpha(int c);
int   isalnum(int c);
int   isspace(int c);
int   isupper(int c);
int   islower(int c);
int   toupper_c(int c);
int   tolower_c(int c);
int   isxdigit(int c);

#endif