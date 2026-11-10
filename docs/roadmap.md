# Roadmap

One step at a time, each on its own branch and pull request, with a
walkthrough and tests. "Lines" is the kernel's code-line count after the
step (`make loc`).

| # | Step | Status | Lines |
|---|---|---|---|
| 1 | **Skeleton:** Makefile, Limine ISO (BIOS and UEFI), serial console, `kprintf`, `panic`, in-kernel tests, `make test`, `make lint`, `make loc`, docs | done ([#1](https://github.com/jamesrlee114/xm3/pull/1)) | 638 |
| 2 | **CPU basics:** GDT, TSS, IDT, exception handlers with a register dump | planned | |
| 3 | **Physical memory:** page-frame allocator from Limine's memory map (bitmap, then buddy) | planned | |
| 4 | **Virtual memory:** 4-level page tables, kernel address space, map and unmap, PCID. Protections: NX, write-xor-execute, SMEP/SMAP, UMIP, guard pages | planned | |
| 5 | **Kernel heap:** slab allocator | planned | |
| 6 | **ACPI and interrupt controllers:** MADT, LAPIC, IOAPIC, timer | planned | |
| 7 | **SMP:** start the other CPUs, per-CPU data, spinlocks (ticket, then MCS), IPIs | planned | |
| 8 | **Threads and scheduler:** context switch, per-CPU run queues, preemption, load balancing, direct switch and lent time slices for IPC | planned | |
| 9 | **User mode:** `syscall`/`sysret`, address spaces, ELF loader, `init` from a boot module | planned | |
| 10 | **IPC:** capabilities, `call`/`reply_recv` in registers, notifications, fast path, shared-memory rings, IPC benchmark against MINIX | planned | |
| 11 | **Servers:** POSIX server (processes, `fork`/`exec`/`wait`, signals), VM server, RAM file system | planned | |
| 12 | **libc and userland:** musl retargeted to xm3 messages, then busybox `sh` | planned | |
| 13 | **User-space drivers:** PCI, MSI-X, interrupts as notifications, virtio-blk, ext2 | planned | |
| 14 | **Networking:** virtio-net with several queues, rings, lwIP server, per-CPU queues, in-process stack option | planned | |
| 15 | **Reliability:** reincarnation server that restarts crashed drivers | planned | |
| 16 | **Linux compatibility (optional):** Linux system calls for unmodified static binaries | planned | |
