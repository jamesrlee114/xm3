/*
 * In-kernel tests. When the command line contains "test", the kernel runs
 * them after booting and reports the result to QEMU through the
 * isa-debug-exit device, so tools/test.py gets a real exit status.
 */
#ifndef XM3_TEST_H
#define XM3_TEST_H

/* Run the tests selected by the command line, then exit QEMU. Words after
 * "test" select failure-path checks: "panic", "ubsan". */
[[noreturn]] void test_run(const char *cmdline);

/* Exit QEMU with a failure status if in test mode; otherwise return. */
void test_fail_exit(void);

#endif
