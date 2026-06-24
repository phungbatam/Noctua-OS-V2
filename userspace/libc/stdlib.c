#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define HEAP_START ((void *)0x400000)
#define HEAP_SIZE  (1024 * 1024)

static char *heap_ptr = NULL;

int atoi(const char *s) {
    int val = 0, sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return sign * val;
}

long atol(const char *s) {
    long val = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return sign * val;
}

void *malloc(size_t size) {
    if (!heap_ptr) {
        heap_ptr = (char *)HEAP_START;
    }
    if (size == 0) return NULL;
    void *ptr = (void *)heap_ptr;
    heap_ptr += size;
    return ptr;
}

void free(void *ptr) {
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    void *newptr = malloc(size);
    if (newptr && ptr) {
        memcpy(newptr, ptr, size);
    }
    return newptr;
}

void abort(void) {
    exit(1);
    for (;;);
}
