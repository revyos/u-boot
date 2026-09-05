#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check the common CPU, IRQ, UART and PMU descriptions in both stage DTBs."""
import subprocess
import sys

control, firmware = sys.argv[1:]

def get(dtb, node, prop, kind="x"):
    return subprocess.check_output(["fdtget", "-t" + kind, dtb, node, prop], text=True).split()

def compare(node, prop, kind="x"):
    assert get(control, node, prop, kind) == get(firmware, node, prop, kind), (node, prop)

for dtb in (control, firmware):
    cpus = subprocess.check_output(["fdtget", "-l", dtb, "/cpus"], text=True).split()
    assert sorted(n for n in cpus if n.startswith("cpu@")) == [f"cpu@{i}" for i in range(8)]
for hart in range(8):
    node = f"/cpus/cpu@{hart}"
    for prop in ("reg", "riscv,cbom-block-size", "riscv,cbop-block-size",
                 "riscv,cboz-block-size", "i-cache-size", "d-cache-size"):
        compare(node, prop)
    for prop in ("compatible", "riscv,isa", "riscv,isa-base", "riscv,isa-extensions", "mmu-type"):
        compare(node, prop, "s")
compare("/cpus", "timebase-frequency")
for node in ("/soc/interrupt-controller@18000000", "/soc/timer@1c000000",
             "/soc/serial@8401000"):
    compare(node, "reg")
    compare(node, "compatible", "s")
for dtb in (control, firmware):
    intcs = {int(get(dtb, f"/cpus/cpu@{h}/interrupt-controller", "phandle")[0], 16): h
             for h in range(8)}
    for node, irqs in (("/soc/interrupt-controller@18000000", (11, 9)),
                       ("/soc/timer@1c000000", (3, 7))):
        spec = [int(v, 16) for v in get(dtb, node, "interrupts-extended")]
        decoded = [(intcs[spec[i]], spec[i + 1]) for i in range(0, len(spec), 2)]
        assert decoded == [(h, irq) for h in range(8) for irq in irqs]
for prop in ("riscv,event-to-mhpmevent", "riscv,event-to-mhpmcounters",
             "riscv,raw-event-to-mhpmcounters"):
    compare("/pmu", prop)
compare("/soc/serial@8401000", "interrupts")
print("PASS: common CPU/PLIC/CLINT/UART/PMU data matches across U-Boot and OpenSBI DTs")
