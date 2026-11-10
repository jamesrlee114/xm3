#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "lib/string.h"

void *memcpy(void *restrict dst, const void *restrict src, size_t n)
{
    cpu_rep_movsb(dst, src, n);
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    /* Copying forwards is safe unless the destination starts inside the
     * source; then copy backwards, from the end. */
    if (((uintptr_t)d - (uintptr_t)s) >= n) {
        cpu_rep_movsb(d, s, n);
    } else {
        while (n > 0U) {
            n--;
            d[n] = s[n];
        }
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    cpu_rep_stosb(dst, (uint8_t)c, n);
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;

    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return (x[i] < y[i]) ? -1 : 1;
        }
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char x = (unsigned char)a[i];
        unsigned char y = (unsigned char)b[i];

        if ((x != y) || (x == 0U)) {
            return (x < y) ? -1 : ((x > y) ? 1 : 0);
        }
    }
    return 0;
}
