/*=============================================================================
 * Standard library utilities implementation
 *=============================================================================*/

#include "stdlib.h"
#include "string.h"

char *itoa(int value, char *str, int base) {
    char *p = str;
    char *p1, tmp;
    unsigned int uvalue;

    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        *p++ = '-';
        uvalue = (unsigned int)(-value);
    } else {
        uvalue = (unsigned int)value;
    }

    char *start = p;
    do {
        int digit = uvalue % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        uvalue /= base;
    } while (uvalue);

    *p = '\0';

    /* Reverse */
    p1 = start;
    p--;
    while (p1 < p) {
        tmp = *p1;
        *p1 = *p;
        *p = tmp;
        p1++;
        p--;
    }

    return str;
}

char *utoa(unsigned int value, char *str, int base) {
    char *p = str;
    char *p1, tmp;

    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    do {
        int digit = value % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    } while (value);

    *p = '\0';

    /* Reverse */
    p1 = str;
    p--;
    while (p1 < p) {
        tmp = *p1;
        *p1 = *p;
        *p = tmp;
        p1++;
        p--;
    }

    return str;
}

char *ltoa(long value, char *str, int base) {
    char *p = str;
    char *p1, tmp;
    unsigned long uvalue;

    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        *p++ = '-';
        uvalue = (unsigned long)(-value);
    } else {
        uvalue = (unsigned long)value;
    }

    char *start = p;
    do {
        int digit = uvalue % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        uvalue /= base;
    } while (uvalue);

    *p = '\0';

    p1 = start;
    p--;
    while (p1 < p) {
        tmp = *p1;
        *p1 = *p;
        *p = tmp;
        p1++;
        p--;
    }

    return str;
}

char *ultoa(unsigned long value, char *str, int base) {
    char *p = str;
    char *p1, tmp;

    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    do {
        int digit = value % base;
        *p++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    } while (value);

    *p = '\0';

    p1 = str;
    p--;
    while (p1 < p) {
        tmp = *p1;
        *p1 = *p;
        *p = tmp;
        p1++;
        p--;
    }

    return str;
}

long strtol(const char *str, char **endptr, int base) {
    long result = 0;
    int sign = 1;

    while (isspace(*str)) str++;

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }

    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9')
            digit = *str - '0';
        else if (*str >= 'a' && *str <= 'z')
            digit = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'Z')
            digit = *str - 'A' + 10;
        else
            break;

        if (digit >= base) break;
        result = result * base + digit;
        str++;
    }

    if (endptr) *endptr = (char *)str;
    return result * sign;
}

unsigned long strtoul(const char *str, char **endptr, int base) {
    unsigned long result = 0;

    while (isspace(*str)) str++;
    if (*str == '+') str++;

    if (base == 0) {
        if (*str == '0') {
            str++;
            if (*str == 'x' || *str == 'X') {
                base = 16;
                str++;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    }

    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9')
            digit = *str - '0';
        else if (*str >= 'a' && *str <= 'z')
            digit = *str - 'a' + 10;
        else if (*str >= 'A' && *str <= 'Z')
            digit = *str - 'A' + 10;
        else
            break;

        if (digit >= base) break;
        result = result * base + digit;
        str++;
    }

    if (endptr) *endptr = (char *)str;
    return result;
}

int atoi(const char *str) {
    return (int)strtol(str, NULL, 10);
}

int abs_val(int x) {
    return x < 0 ? -x : x;
}

int isdigit(int c)  { return c >= '0' && c <= '9'; }
int isalpha(int c)  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c)  { return isalpha(c) || isdigit(c); }
int isspace(int c)  { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isupper(int c)  { return c >= 'A' && c <= 'Z'; }
int islower(int c)  { return c >= 'a' && c <= 'z'; }
int toupper_c(int c){ return islower(c) ? c - 32 : c; }
int tolower_c(int c){ return isupper(c) ? c + 32 : c; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }