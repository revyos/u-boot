/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CONFIG_A210_EVB_H
#define __CONFIG_A210_EVB_H

#define CFG_EXTRA_ENV_SETTINGS \
	"kernel_addr_r=0x82000000\0" \
	"kernel_comp_addr_r=0x8a000000\0" \
	"kernel_comp_size=0x6000000\0" \
	"fdt_addr_r=0x9ff00000\0" \
	"ramdisk_addr_r=0x90000000\0" \
	"pxefile_addr_r=0x88000000\0" \
	"\0"
#endif /* __CONFIG_A210_EVB_H */
