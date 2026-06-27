#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#define EOF (-1)

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int printf(const char *fmt, ...);
int fprintf(int fd, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, int size, const char *fmt, ...);
int vsnprintf(char *buf, int size, const char *fmt, va_list args);
int putchar(int c);
int puts(const char *s);
int getchar(void);
char *gets(char *s);
void perror(const char *s);
int fflush(int fd);

#endif