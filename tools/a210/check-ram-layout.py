#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Ensure the complete RAM package cannot be overwritten by SPL BSS."""
from pathlib import Path
import os
import subprocess
import sys

out = Path(sys.argv[1])
config = {}
for line in (out / "u-boot/.config").read_text().splitlines():
    if line.startswith("CONFIG_") and "=" in line:
        key, value = line.split("=", 1)
        config[key] = value
base = int(config["CONFIG_SPL_TEXT_BASE"], 0) - 0x800
end = base + (out / "dist/binman-spl-with-fit-rvbl.bin").stat().st_size
bss = int(config["CONFIG_SPL_BSS_START_ADDR"], 0)
bss_limit = bss + int(config["CONFIG_SPL_BSS_MAX_SIZE"], 0)
stack = int(config["CONFIG_SPL_STACK"], 0)
nm = os.environ.get("CROSS_COMPILE", "riscv64-linux-gnu-") + "nm"
symbols = {}
for line in subprocess.check_output([nm, str(out / "u-boot/spl/u-boot-spl")], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3 and fields[2] in {"__bss_start", "__bss_end"}:
        symbols[fields[2]] = int(fields[0], 16)
if not (base < end <= bss == symbols["__bss_start"] <= symbols["__bss_end"] <= bss_limit < stack):
    raise SystemExit(f"Unsafe RAM layout: package [{base:#x},{end:#x}), BSS {bss:#x}")
print(f"PASS: RAM package ends at {end:#x}, before SPL BSS {bss:#x}; "
      f"{stack-bss_limit:#x} bytes remain below the initial stack")
