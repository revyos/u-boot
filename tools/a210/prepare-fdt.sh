#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Transform a COPY of the pinned vendor OpenSBI DTB. Never edit the input.
set -eu
if [ "$#" -ne 2 ] || [ "$(realpath -m -- "$1")" = "$(realpath -m -- "$2")" ]; then
	echo "Usage: $0 vendor.dtb output.dtb (different paths)" >&2
	exit 1
fi
cp -- "$1" "$2"
dtb=$2
for hart in 0 1 2 3 4 5 6 7; do
	node=/cpus/cpu@$hart
	fdtget "$dtb" "$node" reg >/dev/null
	fdtput -d "$dtb" "$node" riscv,isa
	fdtput -ts "$dtb" "$node" riscv,isa-extensions \
		i m a f d c v zicntr zicsr zifencei zihpm zba zbb zbc zbs \
		svpbmt sscofpmf sstc zicbom zicbop zicboz
	for feature in cbom cbop cboz; do
		fdtput -tu "$dtb" "$node" "riscv,$feature-block-size" 64
	done
done
# Use upstream T-Head timer quirks: 32-bit compare accesses, time CSR.
fdtput -ts "$dtb" /soc/clint@001c000000 compatible thead,c900-clint
python3 "$(dirname -- "$0")/prepare-plic.py" "$dtb"
# The generic reset-sample interface was removed from OpenSBI in 2023.
# Secondary release uses the A210 platform's fixed register layout.
fdtput -r "$dtb" /soc/reset-sample
