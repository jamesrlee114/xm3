# xm3 design

xm3 is a small, fast, POSIX-like operating system for x86-64 PCs with
several CPUs. It follows the design of MINIX 3: a small kernel, with
servers and drivers running as ordinary user processes that talk by
passing messages. It is written from scratch rather than derived from
MINIX's code.

This document records the decisions and the reasons for them. The order
of work is in [roadmap.md](roadmap.md), and the coding rules are in
[coding-standard.md](coding-standard.md).

## Goals

1. **Fast IPC.** In a microkernel almost everything is a message, so IPC
   speed sets the speed of the whole system.
2. **Small.** The kernel stays at 10,000 lines or fewer; the kernel plus
   core servers at 25,000 or fewer. The build enforces both.
3. **SMP from the start.** Every design decision assumes several CPUs.
4. **Fast networking.**
5. **Secure coding,** aligned with MISRA C and CERT C and checked by tools.
6. **Runs POSIX software:** musl and busybox first, with Linux binary
   compatibility as a later, optional layer.

Non-goals: other CPU architectures, legacy PC hardware, configuration
options, and POSIX certification.

## Why not clean up MINIX 3?

- Its kernel is i386-only and assumes 32-bit throughout: segmentation, the
  4 GB layout, the i386 trap paths.
- Its SMP support is experimental.
- Its userland and build system are NetBSD's.

Porting it to x86-64 would mean rewriting most of the kernel anyway.
Keeping MINIX's architecture and writing new code is cleaner. A MINIX 3
build with tracing and debugging tools is kept alongside as a reference
(`~/minix`, `container/minix.sh trace` and `gdb`).

## Architecture

| Area | MINIX 3 | xm3 |
|---|---|---|
| CPU | i386 | x86-64 only. Kernel at `0xffffffff80000000`; all physical memory mapped (the HHDM) |
| CPUs | Experimental SMP | SMP from the start: per-CPU data via `GS`, x2APIC, IOAPIC, IPIs, TLB shootdown |
| IPC | send/receive/sendrec/notify; 64-byte messages copied through the kernel | `call`/`reply_recv`, messages in registers, direct switch, a fast path, capabilities, PCID; shared memory and rings for bulk data |
| Services | PM, VFS, VM, RS, drivers | A POSIX server (PM and VFS together at first), VM, RS, drivers, network |
| Programs | NetBSD libc and userland | musl and busybox; Linux compatibility later |
| Build | `build.sh`, NetBSD make, LLVM 3.6 cross compiler built from source | One Makefile, the host's clang and lld |
| Timers | PIT (8253) | LAPIC timer (TSC-deadline where available) |

### The kernel provides mechanisms only

The kernel implements IPC, capabilities, address spaces, threads and
scheduling, and the delivery of interrupts as notifications. Everything
else, including process management, file systems, drivers, networking and
all policy, runs in user-space servers.

### How programs talk to xm3

The kernel has no POSIX system calls. As in MINIX, libc turns each POSIX
call into a message sent straight to the server responsible: `read` goes
to the file server, `fork` to the process server. (Compare MINIX's
`minix/lib/libc/sys/read.c`.)

- **libc is musl,** with its system-call layer replaced by xm3 messages.
  **The userland is busybox.** Porting is limited to these and to test
  suites.
- **Linux compatibility comes later** as an optional server that implements
  Linux's system calls for unmodified static Linux binaries, as FreeBSD's
  Linux layer and WSL1 do. It's a compatibility feature, not the
  foundation: emulating another OS's interface never finishes (see WSL1
  and Wine), and xm3's own interface can be faster, with one hop instead
  of two.
- **Conformance** is measured with the Open POSIX Test Suite.

## IPC

IPC is the kernel's hot path, and the rest of the design bends to it. This
follows the lessons of L4 and seL4.

**What MINIX spends per message,** measured by tracing `ls /` on MINIX 3:
- copying the 64-byte message into and out of the kernel (about 3,800
  calls of each copy function)
- validating the endpoint (3,950 calls)
- run-queue work
- switching address space with a TLB flush
- a separate kernel entry for a server's reply and for its next receive.

**xm3's synchronous IPC:**
1. **`call`** (send, then wait for the reply) and **`reply_recv`** (reply,
   then wait for the next request). A round trip is two kernel entries.
2. **Short messages travel in registers:** 6 to 8 words through
   `syscall`/`sysret`, without touching memory. Longer messages use a
   per-thread buffer that is mapped once.
3. **Direct switch:** on `call`, the kernel switches straight to the
   waiting server thread, which runs on the caller's time slice. The
   scheduler isn't involved.
4. **A fast path:** short, lock-free code for the common case (the receiver
   is waiting, on the same CPU, and the message fits in registers).
   Everything else falls back to the general path. The target is a round
   trip of a few hundred CPU cycles.
5. **Capabilities:** a server is named by an index into the caller's
   capability table. Checking it is one bounds check and one load.
6. **PCID,** so switching address space doesn't flush the TLB. IPC never
   saves floating-point state.

**Bulk and cross-CPU data:**

7. **Shared memory, not copies.** Messages carry references to data in
   memory that both sides have mapped.
8. **Shared rings, like io_uring and virtio.** A producer and a consumer
   share submission and completion rings. While both sides are running,
   no kernel entry is needed at all; a notification wakes a side only when
   it is asleep.
9. **Calls stay on one CPU.** A synchronous call to another CPU needs an
   inter-processor interrupt (about 1–2 µs). Busy servers run one thread
   per CPU, and cross-CPU work uses rings.

An IPC benchmark (cycles per round trip on one CPU and across CPUs) runs
as part of `make test`, so a regression is noticed immediately.

## Networking

Drivers run in user space, so the kernel is never on the packet path. This
is kernel bypass by design. Speed comes from avoiding a message per packet:

- **Shared-memory descriptor rings** between the network card driver, the
  network stack and programs, with no copying.
- **Batching and polling,** with an interrupt only when a ring goes from
  empty to non-empty.
- **One queue per CPU,** using the card's multiple queues (RSS) and MSI-X,
  so CPUs don't contend.
- **Two levels of stack:** a network server (starting with lwIP) for normal
  sockets, and a stack linked into the program, talking to the driver's
  rings directly, for programs that need the most speed.

## Small by design

**MINIX 3 for comparison** (code lines, approximate):

| Component | Lines |
|---|---|
| Kernel | 18,000 |
| VFS | 10,900 |
| VM | 8,200 |
| libsys | 5,800 |
| RS | 4,700 |
| PM | 2,800 |

That's about 50,000 lines for the kernel and core services. seL4's kernel
is about 10,000 lines.

**Budgets:**
- the kernel: 10,000 lines or fewer, aiming for 6,000–8,000
- the kernel plus core servers: 25,000 lines or fewer.

`make loc` reports the counts, and `make test` fails if a budget is
exceeded.

**Rules:**
1. Mechanisms only in the kernel.
2. x86-64 only: no portability layers and no `#ifdef` per CPU type.
3. No configuration options: one build, one behaviour.
4. One way to do each thing: one message format, one ring format, one
   capability system.
5. Reuse Limine, musl, busybox and lwIP as pinned upstream copies (not
   counted in the budget).
6. No legacy hardware. Only APIC, MSI/MSI-X, virtio, ACPI and the serial
   port.
7. Delete code that is untested or unused.
8. The only deliberate duplication is the IPC fast path (about 100–200
   lines), tested against the general path.

## Build and tools

- **The compiler is the host's clang,** a cross compiler already
  (`--target=x86_64-unknown-elf`), with `ld.lld`. No toolchain is built and
  no container is needed. `make compdb` gives clangd correct paths, and
  QEMU with KVM runs on the same machine.
- **One non-recursive GNU Makefile.** Make is the most transparent option
  while learning; Meson is the step up if ports grow large.
- **The C version is C23 with an allow-list** (see
  [coding-standard.md](coding-standard.md)). The Makefile checks clang's
  major version and turns `-Werror` off, with a warning, if it differs from
  the tested one, so a compiler update never blocks the build.
- **A container is not used yet.** A pinned-clang container, with the repo
  mounted at the same path, will come when CI does.
- **Testing:** `make test` boots a test kernel in QEMU with BIOS and with
  UEFI. The kernel runs its tests and reports the result through QEMU's
  `isa-debug-exit` device, so the test's exit status comes from the kernel.

## Booting: Limine

[Limine](https://github.com/Limine-Bootloader/Limine) starts the kernel in
64-bit mode at its high address. It provides:
- a memory map
- all physical memory mapped (the "HHDM")
- the other CPUs, ready to start
- boot modules
- a framebuffer
- the kernel command line.

The real-mode and long-mode setup code it replaces is what MINIX does by
hand in `pre_init.c` and `mpx.S`. Writing that code teaches little, so it
can come later as an optional step. xm3 requests Limine base revision 6.
Limine's binaries and `limine.h` are downloaded at pinned versions and
checked against SHA-256 sums.

## Secure coding

xm3 follows MISRA C and CERT C, enforced by compiler warnings, clang-tidy,
cppcheck's MISRA add-on and UBSan. The details, and every deviation, are in
[coding-standard.md](coding-standard.md). CPU protections arrive with
paging (step 4): NX, write-xor-execute, SMEP/SMAP, UMIP and guard pages.
The kernel's segments are already write-xor-execute (see
`kernel/linker.ld`).
