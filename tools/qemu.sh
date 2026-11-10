#!/usr/bin/env bash
#
# Boot an xm3 ISO in QEMU with the serial console on this terminal.
# Quit with Ctrl-A X.
#
# Usage: tools/qemu.sh <iso>
#
# Environment:
#   KVM=0    use emulation (TCG) instead of KVM
#   UEFI=1   boot with OVMF (UEFI) instead of BIOS
#   SMP=n    number of CPUs (default 4)
#   MEM=size memory (default 512M)
#   DEBUG=1  emulation, log interrupts and CPU resets to build/qemu.log
#   GDB=1    stop at the first instruction and wait for gdb on :1234

set -euo pipefail

iso=${1:?usage: tools/qemu.sh <iso>}
args=(
	-machine q35
	-m "${MEM:-512M}"
	-smp "${SMP:-4}"
	-cdrom "$iso"
	-serial mon:stdio
	-display none
	-no-reboot
	-device isa-debug-exit,iobase=0xf4,iosize=0x04
)

if [ "${DEBUG:-0}" = 1 ]; then
	KVM=0
	args+=(-d int,cpu_reset -D build/qemu.log)
fi
if [ "${KVM:-1}" = 1 ] && [ -w /dev/kvm ]; then
	args+=(-accel kvm -cpu host)
else
	args+=(-accel tcg -cpu max)
fi
if [ "${UEFI:-0}" = 1 ]; then
	args+=(-drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/ovmf/OVMF_CODE.fd)
fi
if [ "${GDB:-0}" = 1 ]; then
	args+=(-s -S)
	echo "Waiting for gdb: gdb build/kernel.elf -ex 'target remote :1234'" >&2
fi

exec qemu-system-x86_64 "${args[@]}"
