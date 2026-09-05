#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
a210_uboot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
a210_out=${A210_BUILD_DIR:-$(dirname -- "$a210_uboot")/firmware-build}
dtb=$a210_out/blobs/a210-firmware.dtb
python3 "$a210_uboot/tools/a210/verify-shared-dt.py" "$a210_out/u-boot/arch/riscv/dts/a210-dev.dtb" "$dtb"
python3 "$a210_uboot/tools/a210/prepare-plic.py" "$dtb" --verify
if fdtget -p "$dtb" /soc/reset-sample >/dev/null 2>&1; then
	echo 'Obsolete reset-sample node remains in firmware DT' >&2
	exit 1
fi
for hart in 0 1 2 3 4 5 6 7; do
	node=/cpus/cpu@$hart
	isa=$(fdtget -ts "$dtb" "$node" riscv,isa-extensions)
	for feature in sstc zicbom zicbop zicboz; do
		case " $isa " in
			*" $feature "*) ;;
			*) echo "Missing $feature on hart$hart" >&2; exit 1 ;;
		esac
	done
	for feature in cbom cbop cboz; do
		test "$(fdtget -tu "$dtb" "$node" "riscv,$feature-block-size")" = 64
	done
done
test "$(fdtget -ts "$dtb" /soc/timer@1c000000 compatible)" = thead,c900-clint
fit=$a210_out/u-boot/binman-riscv-boot.itb
test "$(fdtget -ts "$fit" /configurations default)" = a210-dev
fdt_count=0
for node in $(fdtget -l "$fit" /images); do
	if [ "$(fdtget -ts "$fit" "/images/$node" type)" = flat_dt ]; then
		fdt_count=$((fdt_count + 1))
	fi
done
test "$fdt_count" = 1
test "$(fdtget -ts "$fit" /configurations/a210-dev fdt)" = fdt-platform
for option in A210_SHARED_FDT OF_BOARD OF_HAS_PRIOR_STAGE OF_OMIT_DTB; do
	grep -q "^CONFIG_$option=y$" "$a210_out/u-boot/.config"
done
if grep -q '^CONFIG_BINMAN_FDT=y$' "$a210_out/u-boot/.config"; then
	fdtget -p "$dtb" /binman >/dev/null
	test -n "$(fdtget -l "$dtb" /binman)"
	echo 'PASS: shared runtime DT retains the binman node and image description'
fi
mkdir -p "$a210_out/verify"
for image in 0 1 2; do
	"$a210_out/u-boot/tools/dumpimage" -T flat_dt -p "$image" \
		-o "$a210_out/verify/image$image.bin" "$fit" >/dev/null
done
cmp "$a210_out/verify/image0.bin" "$dtb"
cmp "$a210_out/verify/image1.bin" "$a210_out/opensbi/platform/generic/firmware/fw_dynamic.bin"
cmp "$a210_out/verify/image2.bin" "$a210_out/u-boot/u-boot-nodtb.bin"
cmp "$a210_out/u-boot/u-boot.bin" "$a210_out/u-boot/u-boot-nodtb.bin"
echo 'PASS: one shared FIT DTB; U-Boot proper payload has no appended control DTB'
# The vendor eMMC layout places this exact FIT at byte offset 0xd0000.
tail -c +851969 "$a210_out/dist/binman-emmc_boot-loader.img" | cmp - "$fit"
echo 'PASS: all eight firmware CPU descriptions, CLINT quirks, FIT payloads and eMMC FIT layout'
python3 "$a210_uboot/tools/a210/check-ram-layout.py" "$a210_out"
python3 "$a210_uboot/tools/a210/text-fingerprint.py" \
	"$a210_out/opensbi/platform/generic/firmware/fw_dynamic.elf" \
	"$a210_out/opensbi/platform/generic/firmware/fw_dynamic.bin"
echo 'Hardware execution and persistent flashing have NOT been performed by this check.'
