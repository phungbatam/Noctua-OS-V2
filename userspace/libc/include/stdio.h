#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdint.h>

int printf(const char *fmt, ...);
int putchar(int c);
int puts(const char *s);
int getchar(void);
char *gets(char *s);

#endif
