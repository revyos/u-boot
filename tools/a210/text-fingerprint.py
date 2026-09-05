#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Print the reference for the debug OpenSBI immutable-text FNV-1a trace."""
import os
from pathlib import Path
import subprocess
import sys

if len(sys.argv) not in (3, 4):
    raise SystemExit("Usage: text-fingerprint.py firmware.elf firmware.bin [firmware.dtb]")
elf, binary = map(Path, sys.argv[1:3])
prefix = os.environ.get("CROSS_COMPILE", "riscv64-linux-gnu-")
symbols = {}
for line in subprocess.check_output([prefix + "nm", str(elf)], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3 and fields[2] in {"_fw_start", "_text_start", "_text_end"}:
        symbols[fields[2]] = int(fields[0], 16)
if "_text_start" not in symbols or "_text_end" not in symbols:
    # PROVIDE symbols can disappear when a non-debug build has no references.
    # The linker script places the complete immutable code in .text.
    sections = subprocess.check_output([prefix + "objdump", "-h", str(elf)], text=True)
    for line in sections.splitlines():
        fields = line.split()
        if len(fields) >= 4 and fields[1] == ".text":
            symbols["_text_start"] = int(fields[3], 16)
            symbols["_text_end"] = symbols["_text_start"] + int(fields[2], 16)
            break
    else:
        raise SystemExit("Firmware ELF has no .text section")
start = symbols["_text_start"] - symbols["_fw_start"]
end = symbols["_text_end"] - symbols["_fw_start"]
data = binary.read_bytes()
if not 0 <= start < end <= len(data):
    raise SystemExit("Invalid firmware text bounds")
fingerprint = 2166136261
for byte in data[start:end]:
    fingerprint = ((fingerprint ^ byte) * 16777619) & 0xffffffff
print(f"OpenSBI text FNV-1a reference: {fingerprint:08x} ({end-start} bytes)")
if len(sys.argv) == 4:
    subprocess.check_call(["fdtput", "-tx", sys.argv[3], "/chosen",
                           "zhihe,firmware-text-fnv1a", f"{fingerprint:x}"])
