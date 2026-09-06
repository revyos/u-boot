// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <fdtdec.h>
#include <axp_pmic.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <dm.h>
#include <dm/root.h>
#include <event.h>
#include <init.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/libfdt.h>
#include <mapmem.h>
#include <timer.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_XPL_BUILD
void sunxi_board_init(void)
{
	int ret;

	ret = axp_init();
	if (!ret)
		ret = axp_set_dcdc1(CONFIG_AXP_DCDC1_VOLT);
	if (!ret)
		ret = axp_set_dcdc2(CONFIG_AXP_DCDC2_VOLT);
	if (ret)
		panic("Power initialization failed (%d)\n", ret);
	udelay(1000);
	ret = sun252i_v861_cpu_set_clock(CONFIG_SYS_CLK_FREQ);
	if (ret)
		panic("CPU clock initialization failed (%d)\n", ret);
	printf("CPU: %u MHz\n", CONFIG_SYS_CLK_FREQ / 1000000);

	gd->ram_base = CFG_SYS_SDRAM_BASE;
	gd->ram_size = sunxi_dram_init();
	if (!gd->ram_size)
		panic("DRAM initialization failed\n");
}
#endif

int board_fdt_blob_setup(void **fdtp)
{
	void *fdt;
	int ret;

	/* The BootROM does not supply a device tree to SPL. */
	if (IS_ENABLED(CONFIG_XPL_BUILD))
		return -EEXIST;
	if (!gd->arch.firmware_fdt_addr)
		return -EINVAL;
	fdt = map_sysmem(gd->arch.firmware_fdt_addr, 0);
	ret = fdt_check_header(fdt);
	if (ret)
		return ret;
	*fdtp = fdt;
	return 0;
}

#ifdef CONFIG_XPL_BUILD
int dm_scan_other(bool pre_reloc_only)
#else
static int sun252i_v861_power_bus_init(void)
#endif
{
	struct uclass *uc;
	int ret;

	/* The CPU uclass binds the boot hart's architectural timer. */
	ret = uclass_get(UCLASS_CPU, &uc);
	if (ret)
		return ret;
	ret = dm_timer_init();
	if (ret)
		return ret;

	/* Always-on regulators can probe as soon as the DM scan finishes. */
	sun252i_v861_i2c2_init();
	return 0;
}

#ifndef CONFIG_XPL_BUILD
EVENT_SPY_SIMPLE(EVT_DM_POST_INIT_F, sun252i_v861_power_bus_init);
EVENT_SPY_SIMPLE(EVT_DM_POST_INIT_R, sun252i_v861_power_bus_init);
#endif

int board_init(void)
{
	return 0;
}
