/*
 * COM1, a 16550-compatible UART at I/O port 0x3f8. QEMU connects it to
 * -serial; on real hardware it's the RS-232 header, if the board has one.
 * Polled (no interrupts): the console must work before interrupts do.
 */
#include <stdint.h>

#include "cpu.h"
#include "serial.h"

static constexpr uint16_t COM1 = 0x3f8;

/* Register offsets from COM1. */
enum {
    UART_DATA = 0,       /* data; divisor low byte when DLAB is set */
    UART_IER = 1,        /* interrupt enable; divisor high byte with DLAB */
    UART_FCR = 2,        /* FIFO control */
    UART_LCR = 3,        /* line control */
    UART_MCR = 4,        /* modem control */
    UART_LSR = 5,        /* line status */
};

static constexpr uint8_t LCR_DLAB = 0x80U;     /* divisor latch access */
static constexpr uint8_t LCR_8N1 = 0x03U;      /* 8 data bits, no parity, 1 stop */
static constexpr uint8_t FCR_ENABLE = 0xc7U;   /* enable and clear FIFOs, 14-byte trigger */
static constexpr uint8_t MCR_DTR_RTS = 0x03U;
static constexpr uint8_t LSR_THR_EMPTY = 0x20U;

void serial_init(void)
{
    outb(COM1 + UART_IER, 0);           /* polled: no interrupts */
    outb(COM1 + UART_LCR, LCR_DLAB);
    outb(COM1 + UART_DATA, 1);          /* divisor 1: 115200 baud */
    outb(COM1 + UART_IER, 0);
    outb(COM1 + UART_LCR, LCR_8N1);
    outb(COM1 + UART_FCR, FCR_ENABLE);
    outb(COM1 + UART_MCR, MCR_DTR_RTS);
}

static void put_raw(char c)
{
    while ((inb(COM1 + UART_LSR) & LSR_THR_EMPTY) == 0U) {
        cpu_pause();
    }
    outb(COM1 + UART_DATA, (uint8_t)c);
}

void serial_putc(char c)
{
    if (c == '\n') {
        put_raw('\r');
    }
    put_raw(c);
}
