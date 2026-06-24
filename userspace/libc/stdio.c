#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define EOF (-1)

static void print_dec(unsigned long val, int sign, int *count) {
    char buf[32];
    int i = 0;
    if (sign && (long)val < 0) {
        putchar('-');
        (*count)++;
        val = -(long)val;
    }
    if (val == 0) {
        putchar('0');
        (*count)++;
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        putchar(buf[--i]);
        (*count)++;
    }
}

static void print_hex(unsigned long val, int upper, int *count) {
    char hex[] = "0123456789abcdef";
    char hex_up[] = "0123456789ABCDEF";
    char *h = upper ? hex_up : hex;
    char buf[16];
    int i = 0;
    if (val == 0) {
        putchar('0');
        (*count)++;
        return;
    }
    while (val > 0) {
        buf[i++] = h[val & 0xf];
        val >>= 4;
    }
    while (i > 0) {
        putchar(buf[--i]);
        (*count)++;
    }
}

int printf(const char *fmt, ...) {
    va_list args;
    int count = 0;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt != '%') {
            putchar(*fmt++);
            count++;
            continue;
        }
        fmt++;
        if (*fmt == 'l') { fmt++; }
        if (*fmt == 'd' || *fmt == 'i') {
            int val = va_arg(args, int);
            print_dec(val, 1, &count);
        } else if (*fmt == 'u') {
            unsigned int val = va_arg(args, unsigned int);
            print_dec(val, 0, &count);
        } else if (*fmt == 'x') {
            unsigned int val = va_arg(args, unsigned int);
            print_hex(val, 0, &count);
        } else if (*fmt == 'X') {
            unsigned int val = va_arg(args, unsigned int);
            print_hex(val, 1, &count);
        } else if (*fmt == 'p') {
            void *ptr = va_arg(args, void *);
            write(STDOUT_FILENO, "0x", 2);
            count += 2;
            print_hex((unsigned long)ptr, 0, &count);
        } else if (*fmt == 's') {
            char *s = va_arg(args, char *);
            if (!s) s = "(null)";
            write(STDOUT_FILENO, s, strlen(s));
            count += strlen(s);
        } else if (*fmt == 'c') {
            char c = (char)va_arg(args, int);
            putchar(c);
            count++;
        } else if (*fmt == '%') {
            putchar('%');
            count++;
        }
        fmt++;
    }
    va_end(args);
    return count;
}

int putchar(int c) {
    char ch = (char)c;
    if (write(STDOUT_FILENO, &ch, 1) < 0)
        return EOF;
    return c;
}

int puts(const char *s) {
    int len = strlen(s);
    if (write(STDOUT_FILENO, s, len) < 0) return EOF;
    if (write(STDOUT_FILENO, "\n", 1) < 0) return EOF;
    return len + 1;
}

int getchar(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return (unsigned char)c;
    return EOF;
}

char *gets(char *s) {
    int i = 0;
    char c;
    while (1) {
        if (read(STDIN_FILENO, &c, 1) != 1) break;
        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            break;
        }
        if (c == '\b' || c == 0x7f) {
            if (i > 0) {
                i--;
                write(STDOUT_FILENO, "\b \b", 3);
            }
            continue;
        }
        s[i++] = c;
        write(STDOUT_FILENO, &c, 1);
    }
    s[i] = '\0';
    return s;
}
