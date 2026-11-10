# Coding standard

xm3's C code is aligned with **MISRA C** and **CERT C**, checked by tools,
with every exception recorded below. That is a working discipline, not a
certification claim.

- **MISRA C:** the *mandatory* and *required* rules. *Advisory* rules are
  followed where they don't add code without adding safety; for example,
  Rule 15.5 (a single exit point per function) is not followed.
- **CERT C:** its rules (not its recommendations), as far as the tools
  below check them.
- The kernel's line budget applies too: a rule that only adds lines is
  questioned.

## Language: C23 with an allow-list

The code is C17-style, plus these C23 features only:

| Feature | Why it's allowed |
|---|---|
| `constexpr` objects | Typed constants instead of `#define`, which MISRA prefers to discourage. File-scope ones must be `static constexpr`: in C23 they otherwise have external linkage |
| `nullptr` | A null pointer constant with its own type, so it can't be mistaken for the integer 0 |
| `static_assert` (no message needed) | Compile-time checks |
| `bool`, `true`, `false` as keywords | No `<stdbool.h>` |
| `[[nodiscard]]`, `[[noreturn]]`, `[[fallthrough]]`, `[[maybe_unused]]`, and the `[[gnu::...]]` attributes | Standard attribute syntax instead of `__attribute__` |

Anything else from C23 needs an entry in this table first. MISRA C:2025's
public descriptions only cover C11/C18, so C23 use counts as deviation D0.

## Enforcement

`make test` runs all of the following. Any finding fails the build.

### 1. Compiler warnings (always on, `-Werror`)

| Flag | MISRA C:2012 | CERT C |
|---|---|---|
| `-pedantic-errors` | 1.1, 1.2: standard C only | MSC00-C |
| `-Wconversion -Wsign-conversion -Wdouble-promotion` | 10.1–10.8: essential types (in part) | INT31-C, FLP34-C |
| `-Wimplicit-fallthrough` | 16.3 | MSC17-C |
| `-Wswitch-default -Wswitch-enum` | 16.4 | MSC01-C |
| `-Wshadow` | 5.3 | DCL01-C |
| `-Wvla` | 18.8 | ARR32-C |
| `-Wcast-qual` | 11.8 | EXP05-C |
| `-Wstrict-prototypes -Wmissing-prototypes -Wmissing-variable-declarations` | 8.2, 8.4 | DCL07-C, DCL40-C |
| `-Werror=implicit-function-declaration -Werror=implicit-int` | 17.3, 8.1 | DCL31-C |
| `-Wundef` | 20.9 | PRE09-C |
| `-Wcomma` | 12.3 | |
| `-Wunreachable-code -Wunused` | 2.1, 2.2 | MSC12-C |
| `-Wformat=2 -Wformat-security` (with `[[gnu::format]]` on `kprintf`) | | FIO30-C, FIO47-C |
| `-Wpointer-arith` | 18.x | |
| `-Wnull-dereference` | | EXP34-C |

Code generation:
- `-ftrivial-auto-var-init=zero`: uninitialised locals read as zero (CERT EXP33-C)
- `-fstrict-flex-arrays=3`: array bounds are taken literally
- `-fstack-protector-strong`: comes once the kernel has a random canary.

### 2. Static analysis (`make lint`)

- **clang-tidy** (`.clang-tidy`, also shown live in your editor by
  clangd): `cert-*`, `bugprone-*`, `misc-*`, `clang-analyzer-*`.
  These checks are switched off:
  - `bugprone-easily-swappable-parameters`: too noisy for low-level code
  - `misc-include-cleaner`: conflicts with `limine.h`'s structure
  - `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`:
    it flags every `memcpy`/`memset` and recommends Annex K functions,
    which a kernel doesn't have
  - `clang-analyzer-valist.Uninitialized`: false positives when a `va_list`
    is passed by pointer (`kernel/lib/printf.c`).
- **cppcheck's MISRA add-on:** the checkable MISRA C:2012 rules.
  - Recorded deviations are listed in `tools/misra-deviations.txt`, each
    naming the one file allowed to deviate, along with the advisory rules
    below.
  - Things to know about cppcheck 2.19:
    - MISRA findings have "style" severity, so `--enable=style` is
      required. Without it the add-on reports nothing, silently.
    - It doesn't know C23's `nullptr` and `constexpr`, so `make lint`
      defines them for cppcheck only. Without that, the MISRA check is
      incomplete.
    - It can't parse clang's own headers, so it uses its built-in knowledge
      of `<stdint.h>` and friends.
    - Without the MISRA rule texts (a paid PDF), messages show only the rule
      number. Look the rule up by number.

A single finding is suppressed in place only with a comment saying why:
`// NOLINT(check-name)` for clang-tidy, or `// cppcheck-suppress id ;
reason` for cppcheck. Confirmed cppcheck false positives so far:
- 7.2 on `INT64_MIN`, which is signed
- 7.4 on a `char` array initialised from a string literal (MISRA allows it;
  the rule is about pointers)
- `constParameterPointer` on the `rep movsb`/`rep stosb` wrappers, where the
  instruction writes through the pointer.

### Advisory rules not followed

These MISRA C:2012 advisory rules are switched off (in
`tools/misra-deviations.txt`). Following them would add code or
indirection without making the kernel safer:

| Rule | What it asks | Why not |
|---|---|---|
| 2.4, 2.5 | No unused tags or macros | Headers define complete interfaces |
| 2.7 | No unused parameters | Callback and ABI signatures (UBSan handlers) |
| 8.7 | Functions used in one file should be `static` | Kernel interfaces are used by later steps |
| 8.9 | Objects used in one function should be defined in it | Limine requests must be at file scope |
| 11.5 | No conversion from `void *` | `memmove` and friends work on `void *` |
| 15.5 | One exit point per function | Early returns are clearer and shorter |
| 17.8 | Don't modify parameters | Copying them first adds lines, not safety |
| 18.4 | No `+`/`-` on pointers | Buffer walking; bounds are checked separately |
| 20.10 | No `#` or `##` | `KASSERT` and the UBSan handler table |

Other advisory rules stay on and are fixed when reported, for example 12.1
(explicit precedence).

### 3. Runtime checking

Test builds use `-fsanitize=undefined`. `kernel/lib/ubsan.c` reports the
problem and its source location, then panics, which fails the test. `make
test CMDLINE="test ubsan"` checks that a deliberate signed overflow is
caught.

## Deviations

Each deviation is confined to the files named.

| ID | Rule | Where | Reason |
|---|---|---|---|
| D0 | MISRA 1.1 (language version) | everywhere | C23 allow-list above |
| D1 | MISRA Dir 4.3 (assembly), 17.3 (cppcheck sees `__asm__` as a call) | `kernel/cpu.h` | Port I/O, `hlt`, `pause` and `rep movsb` can't be written in C. One instruction per wrapper function |
| D2 | MISRA 11.4, 11.6 (pointer/integer conversion) | `kernel/main.c`, `kernel/test.c`, `kernel/lib/string.c` | Physical addresses, the HHDM offset and `%p` are integers that become pointers or the reverse. Later steps add page tables and device registers |
| D3 | MISRA 17.1 (`<stdarg.h>`) | `kernel/lib/printf.c`, `kernel/kernel.h` | `kprintf`, `ksnprintf` and `panic` are variadic. The compiler checks every call's format string (`[[gnu::format]]`) |
| D4 | MISRA 21.2 (reserved names), 8.14 (`restrict`) | `kernel/lib/string.h`, `kernel/lib/string.c` | A freestanding kernel must define `memcpy`, `memmove`, `memset` and `memcmp` with their standard signatures, because clang emits calls to them |
| D5 | MISRA 21.2, CERT DCL37-C (reserved identifiers); unused structure members | `kernel/lib/ubsan.c` | The `__ubsan_handle_*` names and their data structures are fixed by the compiler's sanitiser ABI |
| D6 | MISRA 8.4, 8.7 (external linkage) | `kernel/main.c` | `kmain` is the entry point, called by Limine, not by C |
| D7 | MISRA 7.2 (unsigned suffix) | `kernel/main.c` | Limine's request IDs (from `limine.h`) are hex constants without a `U` suffix. The request structures are also `volatile`, because the bootloader writes them before the kernel runs |

## Style

- **Formatting:** 4-space indent, braces on all `if`/`for`/`while` bodies
  (MISRA 15.6), and K&R braces, with function braces on their own line.
- **Names:** `snake_case` for functions and variables, `UPPER_CASE` for
  constants and macros.
- **Headers:** a guard named `XM3_<FILE>_H`. Each header states what it
  provides in a comment at the top.
- **Integers:** `<stdint.h>` types for sizes that matter, unsigned
  literals (`0U`) when compared with unsigned values, and explicit casts
  where a conversion narrows.
- **Discarded results:** an ignored return value is written `(void)f()`
  (MISRA 17.7).
- **Comments:** explain *why*, and reference MINIX or the hardware manual
  where that helps.
