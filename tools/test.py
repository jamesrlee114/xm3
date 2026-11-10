#!/usr/bin/env python3
"""Boot an xm3 test ISO in QEMU and report the kernel's verdict.

The kernel, booted with "test" on its command line, runs its tests and
exits QEMU through the isa-debug-exit device: QEMU exit status 1 means the
tests passed, 3 means they failed. The serial console is shown as it runs.

Each ISO is booted with BIOS and with UEFI (OVMF), using KVM if available.

Exit status: 0 if every boot passed, 1 otherwise.
"""

import argparse
import os
import re
import subprocess
import sys

OVMF = "/usr/share/edk2/ovmf/OVMF_CODE.fd"
PASSED = 1      # kernel wrote 0 to the debug-exit port
PASS_MARKER = b"xm3: tests passed"
ANSI = re.compile(r"\x1b\[[0-9;?=]*[A-Za-z]")   # OVMF's terminal control codes


def boot(iso, firmware, timeout):
    cmd = [
        "qemu-system-x86_64", "-machine", "q35", "-m", "512M", "-smp", "4",
        "-cdrom", iso, "-serial", "stdio", "-display", "none",
        "-monitor", "none", "-no-reboot",
        "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
    ]
    if os.access("/dev/kvm", os.R_OK | os.W_OK):
        cmd += ["-accel", "kvm", "-cpu", "host"]
    else:
        cmd += ["-accel", "tcg", "-cpu", "max"]
    if firmware == "uefi":
        cmd += ["-drive", f"if=pflash,format=raw,readonly=on,file={OVMF}"]

    print(f"== {os.path.basename(iso)} ({firmware})", flush=True)
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              stdin=subprocess.DEVNULL, timeout=timeout)
        output, status = proc.stdout, proc.returncode
    except subprocess.TimeoutExpired as e:
        output, status = e.stdout or b"", None
    sys.stdout.write(ANSI.sub("", output.decode("utf-8", "replace").replace("\r", "")))

    if status is None:
        print(f"FAIL: no result after {timeout}s (hung?)")
        return False
    if status == PASSED and PASS_MARKER in output:
        return True
    print(f"FAIL: QEMU exit status {status}")
    return False


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("iso")
    parser.add_argument("--firmware", choices=["bios", "uefi", "both"], default="both")
    parser.add_argument("--timeout", type=int, default=60)
    args = parser.parse_args()

    firmwares = ["bios", "uefi"] if args.firmware == "both" else [args.firmware]
    results = [boot(args.iso, fw, args.timeout) for fw in firmwares]
    print("PASS" if all(results) else "FAIL")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
