#ifndef _ASSERT_H
#define _ASSERT_H

#include <stdio.h>
#include <unistd.h>

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) do { \
    if (!(expr)) { \
        printf("Assertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
        _exit(1); \
    } \
} while (0)
#endif

#endif