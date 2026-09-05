#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Convert the copied single-die firmware DT to the T-Head PLIC binding."""
import subprocess
import sys

dtb = sys.argv[1]
verify = len(sys.argv) == 3 and sys.argv[2] == "--verify"

def get(node, prop, kind="x"):
    return subprocess.check_output(["fdtget", "-t" + kind, dtb, node, prop], text=True).split()

def cells(node, prop):
    return [int(v, 16) for v in get(node, prop)]

def put(node, prop, values, kind="x"):
    subprocess.check_call(["fdtput", "-t" + kind, dtb, node, prop,
                           *[format(v, "x") if isinstance(v, int) else v for v in values]])

nodes = {}

def walk(node, parent=None):
    props = set(subprocess.check_output(["fdtget", "-p", dtb, node], text=True).split())
    nodes[node] = {"props": props, "parent": parent}
    for child in subprocess.check_output(["fdtget", "-l", dtb, node], text=True).split():
        walk(node.rstrip("/") + "/" + child, node)

walk("/")
controllers = {}
plics = []
for node, desc in nodes.items():
    props = desc["props"]
    if "phandle" in props and "#interrupt-cells" in props:
        controllers[cells(node, "phandle")[0]] = (node, cells(node, "#interrupt-cells")[0])
    if "compatible" in props:
        compat = get(node, "compatible", "s")
        if any(c in compat for c in ("riscv,plic0", "zhihe,a210-plic", "thead,c900-plic")):
            plics.append(node)
assert len(plics) == 1, "Expected one PLIC on single-die A210"
plic = plics[0]
phandle = cells(plic, "phandle")[0]
old_count = cells(plic, "#interrupt-cells")[0]
assert old_count in (1, 2)

def parent_irq(node):
    while node:
        if "interrupt-parent" in nodes[node]["props"]:
            return cells(node, "interrupt-parent")[0]
        node = nodes[node]["parent"]
    return None

def convert(spec):
    assert len(spec) % old_count == 0
    if old_count == 1:
        return [v for irq in spec for v in (irq, 4)]
    assert all(spec[i] == 4 for i in range(1, len(spec), 2)), "Unexpected trigger"
    return spec

count = 0
for node, desc in nodes.items():
    props = desc["props"]
    if "interrupts" in props and parent_irq(node) == phandle:
        spec = cells(node, "interrupts")
        new = convert(spec)
        count += len(spec) // old_count
        if verify:
            assert old_count == 2 and spec == new
        elif spec != new:
            put(node, "interrupts", new)
    if "interrupts-extended" in props:
        spec = cells(node, "interrupts-extended")
        result = []
        pos = 0
        while pos < len(spec):
            controller = spec[pos]
            width = controllers[controller][1]
            entry = spec[pos + 1:pos + 1 + width]
            assert len(entry) == width
            if controller == phandle:
                count += 1
                entry = convert(entry)
                if verify:
                    assert old_count == 2
            result.extend([controller, *entry])
            pos += width + 1
        if not verify and result != spec:
            put(node, "interrupts-extended", result)

assert count, "No PLIC consumers found"
if verify:
    assert get(plic, "compatible", "s") == ["zhihe,a210-plic", "thead,c900-plic"]
    assert old_count == 2
    assert cells(plic, "#address-cells") == [0]
else:
    put(plic, "compatible", ["zhihe,a210-plic", "thead,c900-plic"], "s")
    put(plic, "#interrupt-cells", [2])
    put(plic, "#address-cells", [0])
for prop in ("reg-names", "riscv,max-priority"):
    if prop in nodes[plic]["props"]:
        assert not verify, "Legacy PLIC property remains: " + prop
        subprocess.check_call(["fdtput", "-d", dtb, plic, prop])
for node, desc in nodes.items():
    if "plic-delegate" in desc["props"]:
        assert not verify, "Legacy PLIC delegation property remains"
        subprocess.check_call(["fdtput", "-d", dtb, node, "plic-delegate"])
print(f"PASS: C900 PLIC, {count} peripheral interrupt specifiers; CPU-local domains unchanged")
