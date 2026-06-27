#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define HEAP_START ((void *)0x400000)
#define HEAP_SIZE  (1024 * 1024)

static char *heap_ptr = NULL;
static unsigned int rand_seed = 1;

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

long long atoll(const char *s) {
    long long val = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        val = val * 10 + (*s++ - '0');
    return sign * val;
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    long val = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    if ((base == 0 || base == 16) && *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (base == 0) {
        base = (*s == '0') ? 8 : 10;
        if (*s == '0') s++;
    }
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        val = val * base + digit;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return sign * val;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return (unsigned long)strtol(nptr, endptr, base);
}

void *malloc(size_t size) {
    if (!heap_ptr) heap_ptr = (char *)HEAP_START;
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
    if (newptr && ptr) memcpy(newptr, ptr, size);
    return newptr;
}

void abort(void) {
    exit(1);
    for (;;);
}

int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }

int rand(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (rand_seed / 65536) % (RAND_MAX + 1);
}

void srand(unsigned int seed) { rand_seed = seed; }