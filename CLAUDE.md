# xm3

A from-scratch x86-64 SMP microkernel OS inspired by MINIX 3. Read
`docs/design.md` and `docs/roadmap.md` before starting a step, and
`docs/coding-standard.md` before writing C.

## How we work

- James is learning OS internals by building xm3 one roadmap step at a time.
  Claude writes each step and then walks James through it, comparing it
  with how MINIX 3 does the same thing. A MINIX 3 checkout with its tools
  is at `~/minix` (`container/minix.sh trace "cmd"` and `container/minix.sh
  gdb`).
- One branch and one pull request per roadmap step. James merges with
  "Rebase and merge"; GitHub deletes the branch.
- Commit when a step is ready. Push only when James asks.
- Write to `logbook/` only when James asks.
- Update `docs/roadmap.md` (status, PR link, line count) as each step lands.

## Rules

- **C23 allow-list only:** `constexpr` objects (file scope: `static
  constexpr`), `nullptr`, `static_assert`, `bool`, and `[[...]]`
  attributes. Anything else needs an entry in `docs/coding-standard.md`.
- **These must pass before committing:**
  - `make` with no warnings (`-Werror`)
  - `make lint` (clang-tidy and the cppcheck MISRA add-on)
  - `make test` (BIOS and UEFI boots, with UBSan)
- **Line budgets:** kernel 10,000 lines (`make loc`). Prefer deleting to
  adding.
- **Deviations:** new inline asm goes in `kernel/cpu.h`. Any MISRA/CERT
  deviation is added to the table in `docs/coding-standard.md` and to
  `tools/misra-deviations.txt`. Suppress lint findings only with a comment
  saying why.
- **x86-64 only.** No portability layers, configuration options or legacy
  devices.
- **Fast IPC first:** don't add work to IPC paths without a benchmark.

## Commands

| Command | What it does |
|---|---|
| `make` | Build `build/xm3.iso` |
| `make run` | Boot it in QEMU, serial on the terminal (`KVM=0`, `UEFI=1`, `DEBUG=1`, `GDB=1`) |
| `make test` | Lint, line budgets, then UBSan test kernel under BIOS and UEFI |
| `make test CMDLINE="test panic"` | Must fail: checks the failure path |
| `make compdb` | `compile_commands.json` for clangd |
