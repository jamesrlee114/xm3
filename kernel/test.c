#include <stdint.h>

#include "cpu.h"
#include "kernel.h"
#include "lib/string.h"
#include "test.h"

/* QEMU's isa-debug-exit device (tools/test.py adds it): writing v makes
 * QEMU exit with status (v << 1) | 1, so 0 means pass (1) and 1 means
 * fail (3). */
static constexpr uint16_t QEMU_DEBUG_EXIT = 0xf4;
static constexpr uint8_t QEMU_EXIT_PASS = 0;
static constexpr uint8_t QEMU_EXIT_FAIL = 1;

static bool test_mode;
static unsigned checks;
static unsigned failures;

void test_fail_exit(void)
{
    if (test_mode) {
        outb(QEMU_DEBUG_EXIT, QEMU_EXIT_FAIL);
    }
}

static void check_str(const char *got, const char *want, int line)
{
    checks++;
    size_t n = strlen(want);

    if ((strlen(got) != n) || (strncmp(got, want, n) != 0)) {
        failures++;
        kprintf("xm3: FAIL test.c:%d: got \"%s\", want \"%s\"\n", line, got, want);
    }
}

#define CHECK_FORMAT(want, ...)                          \
    do {                                                 \
        char out_[64];                                   \
        (void)ksnprintf(out_, sizeof out_, __VA_ARGS__); \
        check_str(out_, (want), __LINE__);               \
    } while (false)

static void test_format(void)
{
    CHECK_FORMAT("-42", "%d", -42);
    CHECK_FORMAT("4000000000", "%u", 4000000000U);
    CHECK_FORMAT("beef", "%x", 0xbeefU);
    CHECK_FORMAT("0000000000001234", "%016lx", 0x1234UL);
    CHECK_FORMAT("   42|ab   |", "%5d|%-5s|", 42, "ab");
    // cppcheck-suppress misra-c2012-7.2 ; false positive: INT64_MIN is signed
    CHECK_FORMAT("-9223372036854775808", "%lld", (long long)INT64_MIN);
    CHECK_FORMAT("123 x 100%", "%zu %c 100%%", (size_t)123, 'x');
    CHECK_FORMAT("0x00000000deadbeef", "%p", (void *)(uintptr_t)0xdeadbeefU);

    char small[4];
    size_t n = ksnprintf(small, sizeof small, "%s", "abcdef");
    check_str(small, "abc", __LINE__);
    checks++;
    if (n != 6U) {
        failures++;
        kprintf("xm3: FAIL ksnprintf returned %zu, want 6\n", n);
    }
}

static void test_string(void)
{
    // cppcheck-suppress misra-c2012-7.4 ; false positive: an array, not a pointer
    char buf[] = "abcdef";

    (void)memmove(&buf[1], &buf[0], 5);   /* overlapping, copies backwards */
    check_str(buf, "aabcde", __LINE__);
    (void)memmove(&buf[0], &buf[1], 5);   /* overlapping, copies forwards */
    check_str(buf, "abcdee", __LINE__);
    (void)memset(&buf[3], 'z', 2);
    check_str(buf, "abczze", __LINE__);
    /* A deliberate partial copy: buf stays NUL-terminated at the end. */
    (void)memcpy(&buf[0], "xy", 2); // NOLINT(bugprone-not-null-terminated-result)
    check_str(buf, "xyczze", __LINE__);
}

/* Failure-path checks: each must make the run fail. */
static void deliberate_failures(const char *cmdline)
{
    if (cmdline_has(cmdline, "panic")) {
        panic("deliberate panic (command line \"test panic\")");
    }
    if (cmdline_has(cmdline, "ubsan")) {
        volatile int32_t big = INT32_MAX;
        /* Deliberate signed overflow: UBSan must catch it. */
        // cppcheck-suppress integerOverflow
        int32_t overflow = big + 1;
        kprintf("xm3: UBSan missed a signed overflow (%d)\n", overflow);
    }
}

void test_run(const char *cmdline)
{
    test_mode = true;
    kprintf("xm3: running tests\n");
    deliberate_failures(cmdline);
    test_format();
    test_string();

    if (failures != 0U) {
        panic("%u of %u checks failed", failures, checks);
    }
    kprintf("xm3: tests passed (%u checks)\n", checks);
    outb(QEMU_DEBUG_EXIT, QEMU_EXIT_PASS);
    cpu_halt_forever();   /* not reached under QEMU */
}
