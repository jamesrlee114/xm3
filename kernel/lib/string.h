/*
 * C library string functions. memcpy, memmove, memset and memcmp are
 * required: clang may emit calls to them (for example to copy a large
 * struct) even with -ffreestanding. strlen and strncmp are for the kernel's
 * own use; MISRA 21.14 forbids memcmp on strings.
 */
#ifndef XM3_STRING_H
#define XM3_STRING_H

#include <stddef.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);

#endif
