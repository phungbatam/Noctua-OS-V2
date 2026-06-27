#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

#define EOF (-1)

struct output_ctx {
    char *buf;
    int index;
    int max;
};

static void putchar_buf(struct output_ctx *ctx, int c) {
    if (ctx->index < ctx->max - 1) {
        ctx->buf[ctx->index] = (char)c;
    }
    ctx->index++;
}

static void putchar_stdout(struct output_ctx *ctx, int c) {
    (void)ctx;
    char ch = (char)c;
    write(STDOUT_FILENO, &ch, 1);
}


struct fmt_cb {
    void (*putchar)(struct output_ctx *, int);
    struct output_ctx *ctx;
};

static void print_dec(unsigned long val, int sign, struct fmt_cb *cb) {
    char buf[32];
    int i = 0;
    if (sign && (long)val < 0) {
        cb->putchar(cb->ctx, '-');
        val = -(long)val;
    }
    if (val == 0) { cb->putchar(cb->ctx, '0'); return; }
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i > 0) cb->putchar(cb->ctx, buf[--i]);
}

static void print_hex(unsigned long val, int upper, struct fmt_cb *cb) {
    const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[16];
    int i = 0;
    if (val == 0) { cb->putchar(cb->ctx, '0'); return; }
    while (val > 0) { buf[i++] = hex[val & 0xf]; val >>= 4; }
    while (i > 0) cb->putchar(cb->ctx, buf[--i]);
}

static void vfprintf_cb(struct fmt_cb *cb, const char *fmt, va_list args) {
    while (*fmt) {
        if (*fmt != '%') { cb->putchar(cb->ctx, *fmt++); continue; }
        fmt++;
        int lflag = 0;
        while (*fmt == 'l') { lflag = 1; fmt++; }
        (void)lflag;
        switch (*fmt) {
            case 'd': case 'i':
                print_dec(va_arg(args, int), 1, cb); break;
            case 'u':
                print_dec(va_arg(args, unsigned int), 0, cb); break;
            case 'x':
                print_hex(va_arg(args, unsigned int), 0, cb); break;
            case 'X':
                print_hex(va_arg(args, unsigned int), 1, cb); break;
            case 'p':
                cb->putchar(cb->ctx, '0');
                cb->putchar(cb->ctx, 'x');
                print_hex((unsigned long)va_arg(args, void *), 0, cb);
                break;
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) cb->putchar(cb->ctx, *s++);
                break;
            }
            case 'c':
                cb->putchar(cb->ctx, (char)va_arg(args, int));
                break;
            case '%':
                cb->putchar(cb->ctx, '%');
                break;
            default:
                cb->putchar(cb->ctx, '%');
                if (*fmt) cb->putchar(cb->ctx, *fmt);
                break;
        }
        if (*fmt) fmt++;
    }
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    struct output_ctx ctx = {NULL, 0, 0};
    struct fmt_cb cb = {putchar_stdout, &ctx};
    vfprintf_cb(&cb, fmt, args);
    va_end(args);
    return ctx.index;
}

int fprintf(int fd, const char *fmt, ...) {
    (void)fd;
    va_list args;
    va_start(args, fmt);
    struct output_ctx ctx = {NULL, 0, 0};
    struct fmt_cb cb = {putchar_stdout, &ctx};
    vfprintf_cb(&cb, fmt, args);
    va_end(args);
    return ctx.index;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, 4096, fmt, args);
    va_end(args);
    return ret;
}

int snprintf(char *buf, int size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}

int vsnprintf(char *buf, int size, const char *fmt, va_list args) {
    struct output_ctx ctx = {buf, 0, size};
    struct fmt_cb cb = {putchar_buf, &ctx};
    vfprintf_cb(&cb, fmt, args);
    if (size > 0) {
        int end = ctx.index < size - 1 ? ctx.index : size - 1;
        buf[end] = '\0';
    }
    return ctx.index;
}

int putchar(int c) {
    char ch = (char)c;
    if (write(STDOUT_FILENO, &ch, 1) < 0) return EOF;
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
    if (read(STDIN_FILENO, &c, 1) == 1) return (unsigned char)c;
    return EOF;
}

char *gets(char *s) {
    int i = 0;
    char c;
    while (1) {
        if (read(STDIN_FILENO, &c, 1) != 1) break;
        if (c == '\n' || c == '\r') { write(STDOUT_FILENO, "\n", 1); break; }
        if (c == '\b' || c == 0x7f) {
            if (i > 0) { i--; write(STDOUT_FILENO, "\b \b", 3); }
            continue;
        }
        s[i++] = c;
        write(STDOUT_FILENO, &c, 1);
    }
    s[i] = '\0';
    return s;
}

void perror(const char *s) {
    if (s && *s) { write(STDERR_FILENO, s, strlen(s)); write(STDERR_FILENO, ": ", 2); }
    write(STDERR_FILENO, "Error\n", 6);
}

int fflush(int fd) {
    (void)fd;
    return 0;
}