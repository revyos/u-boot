#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
a210_uboot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
a210_root=$(dirname -- "$a210_uboot")
a210_out=${A210_BUILD_DIR:-$a210_root/firmware-build}
a210_sbi=${A210_OPENSBI_DIR:-$a210_root/opensbi-a210-v1.9}
a210_blobs=$a210_uboot/tools/a210/blobs
a210_cross=${CROSS_COMPILE:-riscv64-linux-gnu-}
a210_jobs=${A210_JOBS:-8}
mkdir -p "$a210_out/opensbi" "$a210_out/u-boot" "$a210_out/blobs" "$a210_out/dist"
if [ -x "$a210_out/host-tools/bin/swig" ]; then
	PATH="$a210_out/host-tools/bin:$PATH"
	export PATH
fi
(cd "$a210_blobs" && sha256sum -c SHA256SUMS)
cp "$a210_blobs/bootzero2.bin" "$a210_blobs/bootzero-rvbl.bin" "$a210_out/blobs/"
make -C "$a210_uboot" O="$a210_out/u-boot" CROSS_COMPILE="$a210_cross" \
	a210_evb_oerv_defconfig
# Build from shared DTS sources before FIT packaging; no vendor DTB import.
make -C "$a210_uboot" O="$a210_out/u-boot" -j"$a210_jobs" \
	CROSS_COMPILE="$a210_cross" dtbs
cp "$a210_out/u-boot/arch/riscv/dts/a210-dev.dtb" \
	"$a210_out/blobs/a210-firmware.dtb"
# CONFIG_BINMAN_FDT also consumes /binman at runtime; retain it in the
# shared tree so U-Boot can initialize its binman information library.
make -C "$a210_sbi" O="$a210_out/opensbi" -j"$a210_jobs" \
	CROSS_COMPILE="$a210_cross" PLATFORM=generic FW_TEXT_START=0x80000000
CROSS_COMPILE="$a210_cross" python3 "$a210_uboot/tools/a210/text-fingerprint.py" \
	"$a210_out/opensbi/platform/generic/firmware/fw_dynamic.elf" \
	"$a210_out/opensbi/platform/generic/firmware/fw_dynamic.bin" \
	"$a210_out/blobs/a210-firmware.dtb"
make -C "$a210_uboot" O="$a210_out/u-boot" -j"$a210_jobs" \
	CROSS_COMPILE="$a210_cross" \
	OPENSBI="$a210_out/opensbi/platform/generic/firmware/fw_dynamic.bin" \
	BINMAN_INDIRS="$a210_out/blobs"
cp "$a210_out/u-boot/binman-emmc_boot-loader.img" \
	"$a210_out/u-boot/binman-spl-with-fit-rvbl.bin" \
	"$a210_out/blobs/bootzero-rvbl.bin" "$a210_out/dist/"
A210_BUILD_DIR="$a210_out" sh "$a210_uboot/tools/a210/verify.sh"
(cd "$a210_out/dist" && sha256sum *.bin *.img)
