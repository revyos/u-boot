#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
if [ "$#" -ne 2 ] || [ "$(realpath -m -- "$1")" = "$(realpath -m -- "$2")" ]; then
	echo "Usage: $0 input-linux.dtb output-test.dtb (different paths)" >&2
	exit 1
fi
cp -- "$1" "$2"
for hart in 0 1 2 3 4 5 6 7; do
	node=/cpus/cpu@$hart
	a210_isa=$(fdtget -ts "$2" "$node" riscv,isa-extensions)
	for feature in sstc zicbom zicbop zicboz; do
		case " $a210_isa " in
			*" $feature "*) ;;
			*) a210_isa="$a210_isa $feature" ;;
		esac
	done
	# Intentional word splitting: fdtput takes one argument per string.
	fdtput -ts "$2" "$node" riscv,isa-extensions $a210_isa
	for feature in cbom cbop cboz; do
		fdtput -tu "$2" "$node" "riscv,$feature-block-size" 64
	done
done
