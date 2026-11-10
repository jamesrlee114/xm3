/*
 * kprintf, ksnprintf and panic, built on one formatter that writes through
 * a "sink": the serial port, or a buffer.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "kernel.h"
#include "lib/string.h"
#include "serial.h"
#include "test.h"

struct sink {
    void (*put)(struct sink *s, char c);
    char *buf;        /* buffer sinks only */
    size_t size;
    size_t len;       /* characters produced, even if they didn't fit */
};

static void put_serial(struct sink *s, char c)
{
    serial_putc(c);
    s->len++;
}

static void put_buffer(struct sink *s, char c)
{
    if ((s->len + 1U) < s->size) {
        s->buf[s->len] = c;
    }
    s->len++;
}

struct spec {
    bool left;        /* '-': pad on the right */
    char pad;         /* ' ' or '0' */
    unsigned width;
};

static void put_padded(struct sink *s, const struct spec *sp, const char *str, size_t n)
{
    size_t fill = (sp->width > n) ? (sp->width - n) : 0U;

    if (!sp->left) {
        for (size_t i = 0; i < fill; i++) {
            s->put(s, sp->pad);
        }
    }
    for (size_t i = 0; i < n; i++) {
        s->put(s, str[i]);
    }
    if (sp->left) {
        for (size_t i = 0; i < fill; i++) {
            s->put(s, ' ');
        }
    }
}

static void put_number(struct sink *s, const struct spec *sp, uint64_t value,
                       unsigned base, bool negative)
{
    static const char digits[] = "0123456789abcdef";
    char tmp[24];     /* 2^64 needs 20 decimal digits, plus the sign */
    size_t n = sizeof tmp;

    do {
        n--;
        tmp[n] = digits[value % base];
        value /= base;
    } while (value != 0U);
    if (negative) {
        n--;
        tmp[n] = '-';
    }
    put_padded(s, sp, &tmp[n], (sizeof tmp) - n);
}

/* Read an argument of the length given by the modifier (none, l, ll, z). */
static uint64_t arg_unsigned(va_list *ap, unsigned longs, bool size)
{
    if (size) {
        return va_arg(*ap, size_t);
    }
    if (longs != 0U) {
        return va_arg(*ap, unsigned long long);
    }
    return va_arg(*ap, unsigned int);
}

static int64_t arg_signed(va_list *ap, unsigned longs, bool size)
{
    if (size || (longs != 0U)) {
        return va_arg(*ap, long long);
    }
    return va_arg(*ap, int);
}

static void format(struct sink *s, const char *fmt, va_list *ap)
{
    const char *p = fmt;

    while (*p != '\0') {
        if (*p != '%') {
            s->put(s, *p);
            p++;
            continue;
        }
        p++;
        struct spec sp = { .left = false, .pad = ' ', .width = 0 };
        while ((*p == '-') || (*p == '0')) {
            if (*p == '-') {
                sp.left = true;
            } else {
                sp.pad = '0';
            }
            p++;
        }
        while ((*p >= '0') && (*p <= '9')) {
            sp.width = (sp.width * 10U) + (unsigned)(*p - '0');
            p++;
        }
        unsigned longs = 0;
        bool size = false;
        while ((*p == 'l') || (*p == 'z')) {
            if (*p == 'l') {
                longs++;
            } else {
                size = true;
            }
            p++;
        }

        switch (*p) {
        case 'd':
        case 'i': {
            int64_t v = arg_signed(ap, longs, size);
            /* Negate in unsigned arithmetic: -INT64_MIN would overflow. */
            uint64_t mag = (v < 0) ? (0U - (uint64_t)v) : (uint64_t)v;
            put_number(s, &sp, mag, 10, v < 0);
            break;
        }
        case 'u':
            put_number(s, &sp, arg_unsigned(ap, longs, size), 10, false);
            break;
        case 'x':
            put_number(s, &sp, arg_unsigned(ap, longs, size), 16, false);
            break;
        case 'p':
            s->put(s, '0');
            s->put(s, 'x');
            sp.pad = '0';
            sp.width = 16;
            put_number(s, &sp, (uintptr_t)va_arg(*ap, void *), 16, false);
            break;
        case 'c': {
            char c = (char)va_arg(*ap, int);
            put_padded(s, &sp, &c, 1);
            break;
        }
        case 's': {
            const char *str = va_arg(*ap, const char *);
            if (str == nullptr) {
                str = "(null)";
            }
            put_padded(s, &sp, str, strlen(str));
            break;
        }
        case '%':
            s->put(s, '%');
            break;
        case '\0':
            return;   /* a lone '%' at the end */
        default:
            /* Unknown conversion: print it as is, so it's visible. */
            s->put(s, '%');
            s->put(s, *p);
            break;
        }
        p++;   /* past the conversion character */
    }
}

void kprintf(const char *fmt, ...)
{
    struct sink s = { .put = put_serial, .buf = nullptr, .size = 0, .len = 0 };
    va_list ap;

    va_start(ap, fmt);
    format(&s, fmt, &ap);
    va_end(ap);
}

/* Like snprintf: always NUL-terminates (if size > 0) and returns the
 * length the full output would have had. */
size_t ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    struct sink s = { .put = put_buffer, .buf = buf, .size = size, .len = 0 };
    va_list ap;

    va_start(ap, fmt);
    format(&s, fmt, &ap);
    va_end(ap);
    if (size > 0U) {
        buf[(s.len < size) ? s.len : (size - 1U)] = '\0';
    }
    return s.len;
}

void panic(const char *fmt, ...)
{
    struct sink s = { .put = put_serial, .buf = nullptr, .size = 0, .len = 0 };
    va_list ap;

    kprintf("\nxm3: PANIC: ");
    va_start(ap, fmt);
    format(&s, fmt, &ap);
    va_end(ap);
    kprintf("\n");
    test_fail_exit();
    cpu_halt_forever();
}
