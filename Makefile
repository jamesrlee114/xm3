# xm3 build. One non-recursive Makefile; host clang and ld.lld are the
# cross compiler. See docs/design.md ("Build") and docs/coding-standard.md.
#
#   make            build build/xm3.iso
#   make run        boot it in QEMU (KVM=0 for emulation, UEFI=1 for OVMF)
#   make test       UBSan build, boot with CMDLINE=test, check the result,
#                   plus lint and line budgets
#   make lint       clang-tidy and cppcheck (MISRA C)
#   make loc        line counts against the budgets
#   make compdb     compile_commands.json for clangd (needs bear)
#   make clean      remove build output (keeps build/download; distclean
#                   removes that too)

# ---- Versions ---------------------------------------------------------------

CLANG_TESTED_MAJOR := 21

LIMINE_VERSION := 12.9.6
LIMINE_SHA256 := 939f670e0831cc940453fd3a0ddfb2b324c0b4e3c5d0e741c190aa0d13df0103
LIMINE_URL := https://github.com/Limine-Bootloader/Limine/releases/download/v$(LIMINE_VERSION)/limine-binary.tar.xz
LIMINE_PROTOCOL_COMMIT := 6e85ddd6f928b5b512015c4d47d1cfce2fd28079
LIMINE_H_SHA256 := 0aadf2633c85d8cb145344c968add158aeb1deb68af567d55ba8431c492b6d4c
LIMINE_H_URL := https://raw.githubusercontent.com/Limine-Bootloader/limine-protocol/$(LIMINE_PROTOCOL_COMMIT)/include/limine.h

# Line budgets (code lines, excluding blanks and comments). Raise them on
# purpose, never by accident.
KERNEL_LOC_BUDGET := 10000

# ---- Configuration ----------------------------------------------------------

BUILD ?= build
CMDLINE ?=
UBSAN ?= 0

CC := clang
LD := ld.lld

# Short build output; make V=1 shows the full commands.
ifeq ($(V),1)
Q :=
else
Q := @
endif
DL := build/download
LIMINE_DIR := $(DL)/limine-$(LIMINE_VERSION)

CLANG_MAJOR := $(shell $(CC) -dumpversion 2>/dev/null | cut -d. -f1)
ifeq ($(CLANG_MAJOR),$(CLANG_TESTED_MAJOR))
WERROR := -Werror
else
$(warning clang $(CLANG_MAJOR) differs from the tested clang $(CLANG_TESTED_MAJOR); -Werror is off)
WERROR :=
endif

# Kernel code model: freestanding, higher half, no FP/SIMD, no red zone.
ARCH_FLAGS := --target=x86_64-unknown-elf -std=c23 -ffreestanding -nostdlibinc \
	-mcmodel=kernel -mno-red-zone -mgeneral-regs-only -fno-pic -fno-pie \
	-fno-stack-protector -fno-omit-frame-pointer \
	-fno-asynchronous-unwind-tables -O2 -g

# MISRA C / CERT C aligned warnings. See docs/coding-standard.md for the
# rule each one maps to.
WARN_FLAGS := -Wall -Wextra -pedantic-errors \
	-Wconversion -Wsign-conversion -Wdouble-promotion \
	-Wimplicit-fallthrough -Wswitch-default -Wswitch-enum \
	-Wshadow -Wvla -Wcast-qual \
	-Wstrict-prototypes -Wmissing-prototypes -Wmissing-variable-declarations \
	-Werror=implicit-function-declaration -Werror=implicit-int \
	-Wundef -Wcomma -Wunreachable-code -Wunused \
	-Wformat=2 -Wformat-security -Wpointer-arith -Wnull-dereference

HARDEN_FLAGS := -ftrivial-auto-var-init=zero -fstrict-flex-arrays=3

ifeq ($(UBSAN),1)
HARDEN_FLAGS += -fsanitize=undefined -fno-sanitize=function
endif

CPPFLAGS := -Ikernel -I$(DL)/include -MMD -MP
CFLAGS := $(ARCH_FLAGS) $(WARN_FLAGS) $(WERROR) $(HARDEN_FLAGS)
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 \
	--no-dynamic-linker -T kernel/linker.ld

SRCS := $(shell find kernel -name '*.c' | sort)
OBJS := $(SRCS:%.c=$(BUILD)/obj/%.o)

# ---- Build ------------------------------------------------------------------

.PHONY: all run test lint loc compdb clean distclean
all: $(BUILD)/xm3.iso

$(BUILD)/obj/%.o: %.c $(DL)/include/limine.h | $(BUILD)/flags
	@mkdir -p $(dir $@)
	@echo '  CC      $<'
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Rebuild everything when the flags change (UBSAN=1 vs 0, a new clang).
$(BUILD)/flags: FORCE
	@mkdir -p $(BUILD)
	@echo '$(CC) $(CLANG_MAJOR) $(CFLAGS) $(LDFLAGS)' | cmp -s - $@ || \
		echo '$(CC) $(CLANG_MAJOR) $(CFLAGS) $(LDFLAGS)' > $@
$(OBJS): $(BUILD)/flags

# The same for the kernel command line, which only changes the ISO.
$(BUILD)/cmdline: FORCE
	@mkdir -p $(BUILD)
	@echo '$(CMDLINE)' | cmp -s - $@ || echo '$(CMDLINE)' > $@

$(BUILD)/kernel.elf: $(OBJS) kernel/linker.ld
	@echo '  LD      $@'
	$(Q)$(LD) $(LDFLAGS) $(OBJS) -o $@

# The ISO boots on both BIOS and UEFI. limine.conf is generated so the
# kernel command line can be set per build: make run CMDLINE="test".
$(BUILD)/xm3.iso: $(BUILD)/kernel.elf boot/limine.conf.in $(LIMINE_DIR)/limine $(BUILD)/cmdline
	@echo '  ISO     $@'
	$(Q)rm -rf $(BUILD)/iso
	$(Q)mkdir -p $(BUILD)/iso/boot/limine $(BUILD)/iso/EFI/BOOT
	$(Q)cp $(BUILD)/kernel.elf $(BUILD)/iso/boot/
	$(Q)sed 's|@CMDLINE@|$(CMDLINE)|' boot/limine.conf.in > $(BUILD)/iso/boot/limine/limine.conf
	$(Q)cp $(addprefix $(LIMINE_DIR)/,limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin) \
		$(BUILD)/iso/boot/limine/
	$(Q)cp $(LIMINE_DIR)/BOOTX64.EFI $(BUILD)/iso/EFI/BOOT/
	$(Q)xorriso -as mkisofs -quiet -R -r -J -V XM3 \
		-b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 \
		-boot-info-table -hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label $(BUILD)/iso -o $@ \
		2>$(BUILD)/xorriso.log || { cat $(BUILD)/xorriso.log; exit 1; }
	$(Q)$(LIMINE_DIR)/limine bios-install --quiet $@

# ---- Pinned downloads (checksummed) -----------------------------------------

$(DL)/limine-binary-$(LIMINE_VERSION).tar.xz:
	@mkdir -p $(DL)
	curl -fsSL -o $@.tmp $(LIMINE_URL)
	echo '$(LIMINE_SHA256)  $@.tmp' | sha256sum -c --quiet
	mv $@.tmp $@

$(LIMINE_DIR)/limine: $(DL)/limine-binary-$(LIMINE_VERSION).tar.xz
	rm -rf $(LIMINE_DIR) && mkdir -p $(LIMINE_DIR)
	tar -xJf $< -C $(LIMINE_DIR) --strip-components=1
	$(MAKE) -C $(LIMINE_DIR) CC=cc limine

$(DL)/include/limine.h:
	@mkdir -p $(dir $@)
	curl -fsSL -o $@.tmp $(LIMINE_H_URL)
	echo '$(LIMINE_H_SHA256)  $@.tmp' | sha256sum -c --quiet
	mv $@.tmp $@

# ---- Run and test -----------------------------------------------------------

run: $(BUILD)/xm3.iso
	tools/qemu.sh $<

# A separate build directory, so the UBSan objects don't replace the normal
# ones. lint and loc run first: they're fast and fail early.
test: lint loc
	$(MAKE) --no-print-directory BUILD=build/test UBSAN=1 CMDLINE='$(or $(CMDLINE),test)'
	tools/test.py build/test/xm3.iso

LINT_FLAGS := -Ikernel -I$(DL)/include $(ARCH_FLAGS) $(WARN_FLAGS)
lint: $(DL)/include/limine.h
	clang-tidy --quiet $(SRCS) -- $(LINT_FLAGS)
	@command -v cppcheck >/dev/null || { echo 'cppcheck not found: sudo dnf install cppcheck' >&2; exit 1; }
	@# MISRA findings are "style" severity. cppcheck doesn't know C23's
	@# nullptr and constexpr yet, so they're defined for it only.
	cppcheck --quiet --error-exitcode=1 --enable=warning,style,portability \
		--std=c23 --addon=misra --inline-suppr \
		--suppressions-list=tools/misra-deviations.txt \
		-Ikernel -I$(DL)/include -D__x86_64__ \
		'-Dnullptr=((void *)0)' -Dconstexpr=const \
		$(SRCS)

loc:
	tools/loc.py --budget kernel=$(KERNEL_LOC_BUDGET) kernel

compdb:
	$(MAKE) clean-objs
	bear -- $(MAKE) --no-print-directory $(BUILD)/kernel.elf

.PHONY: clean-objs
clean-objs:
	rm -rf $(BUILD)/obj

clean:
	rm -rf build/obj build/test build/iso build/kernel.elf build/xm3.iso build/flags build/cmdline

distclean:
	rm -rf build

.PHONY: FORCE
FORCE:

-include $(OBJS:.o=.d)
