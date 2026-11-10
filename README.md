# xm3

A small, fast, POSIX-like microkernel operating system for x86-64 PCs with
several CPUs, inspired by MINIX 3 and written from scratch in C23.

**Status:** step 1 of the [roadmap](docs/roadmap.md). It boots on BIOS and
UEFI, prints to the serial console and runs its own tests. See
[docs/design.md](docs/design.md) for the design.

## Requirements (Fedora)

```bash
sudo dnf install clang lld bear cppcheck xorriso qemu-system-x86 edk2-ovmf python3
```

`make` downloads Limine (the bootloader) at a pinned version into
`build/download/` and checks its SHA-256 sum.

## Build and run

```bash
make            # build build/xm3.iso
make run        # boot it in QEMU; quit with Ctrl-A X
make test       # lint, line budgets, then boot the test kernel (BIOS and UEFI)
```

`make run` options:

| Option | What it does |
|---|---|
| `KVM=0` | Emulation (TCG) instead of KVM |
| `UEFI=1` | Boot with UEFI (OVMF) instead of BIOS |
| `SMP=8`, `MEM=1G` | Number of CPUs and amount of memory |
| `DEBUG=1` | Emulation, with interrupts and CPU resets logged to `build/qemu.log` |
| `GDB=1` | Stop at the first instruction and wait for `gdb build/kernel.elf -ex 'target remote :1234'` |
| `CMDLINE="..."` | Kernel command line |

## Repository layout

| Path | Contents |
|---|---|
| `kernel/` | The kernel. `main.c` is the entry point; `cpu.h` holds all inline assembly |
| `kernel/lib/` | `kprintf`, string functions, UBSan handlers |
| `boot/` | Limine configuration template |
| `tools/` | `qemu.sh` (run), `test.py` (test harness), `loc.py` (line budgets), MISRA deviations |
| `docs/` | Design, roadmap and coding standard |
| `logbook/` | Development notes |

## Editor setup

`make compdb` writes `compile_commands.json` for clangd. clangd reads
`.clang-tidy`, so your editor shows the same checks as `make lint`.
