/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __CONFIG_SUN252I_H
#define __CONFIG_SUN252I_H

#define CFG_SYS_SDRAM_BASE	0x40000000

#define CFG_EXTRA_ENV_SETTINGS \
	"kernel_addr_r=0x42000000\0" \
	"fdt_addr_r=0x43000000\0" \
	"scriptaddr=0x43100000\0" \
	"pxefile_addr_r=0x43200000\0" \
	"ramdisk_addr_r=0x43400000\0"

#endif
