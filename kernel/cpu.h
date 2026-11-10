/*
 * Small wrappers around x86-64 instructions that C can't express.
 *
 * MISRA C Dir 4.3 deviation (D1 in docs/coding-standard.md): inline
 * assembly is only allowed here, one instruction per function, so the rest
 * of the kernel is plain C.
 */
#ifndef XM3_CPU_H
#define XM3_CPU_H

#include <stddef.h>
#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port) : "memory");
    return value;
}

/* Spin-wait hint: lets the other hyperthread run and saves power. */
static inline void cpu_pause(void)
{
    __asm__ volatile("pause");
}

/* Stop this CPU for good: interrupts off, then halt (in a loop, because a
 * non-maskable interrupt can still wake it). */
[[noreturn]] static inline void cpu_halt_forever(void)
{
    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}

/* memcpy/memset in one instruction each. With "fast short rep movsb"
 * (ERMS/FSRM), present on CPUs from the last decade, these are as fast as
 * hand-written loops and much smaller. */
// cppcheck-suppress constParameterPointer ; the instruction writes through dst
static inline void cpu_rep_movsb(void *dst, const void *src, size_t n)
{
    __asm__ volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
}

// cppcheck-suppress constParameterPointer ; the instruction writes through dst
static inline void cpu_rep_stosb(void *dst, uint8_t value, size_t n)
{
    __asm__ volatile("rep stosb" : "+D"(dst), "+c"(n) : "a"(value) : "memory");
}

#endif
