/* Kernel-wide basics: console output, panic and assertions. */
#ifndef XM3_KERNEL_H
#define XM3_KERNEL_H

#include <stddef.h>

/*
 * printf-style formatting, a small subset: %s %c %d %i %u %x %p %%, the
 * flags '0' and '-', a field width, and the length modifiers l, ll and z.
 * The format attribute makes the compiler check every call's arguments.
 *
 * MISRA C Rule 17.1 deviation (D3): variadic functions, for formatting
 * only.
 */
[[gnu::format(printf, 1, 2)]] void kprintf(const char *fmt, ...);
[[gnu::format(printf, 3, 4)]] size_t ksnprintf(char *buf, size_t size,
                                               const char *fmt, ...);

/* Print a message and stop. In test mode, also tell QEMU the test failed. */
[[noreturn, gnu::format(printf, 1, 2)]] void panic(const char *fmt, ...);

/* True if the space-separated command line contains word. */
bool cmdline_has(const char *cmdline, const char *word);

#define KASSERT(cond)                                                       \
    do {                                                                    \
        if (!(cond)) {                                                      \
            panic("assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__); \
        }                                                                   \
    } while (false)

#endif
