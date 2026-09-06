// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <axp_pmic.h>
#include <asm/arch/cpu.h>
#include <asm/global_data.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_XPL_BUILD
void sunxi_board_init(void)
{
	int ret;

	ret = axp_init();
	if (!ret)
		ret = axp_set_dcdc2(CONFIG_AXP_DCDC2_VOLT);
	if (ret)
		panic("DDR power initialization failed (%d)\n", ret);
	udelay(1000);

	gd->ram_base = CFG_SYS_SDRAM_BASE;
	gd->ram_size = sunxi_dram_init();
	if (!gd->ram_size)
		panic("DRAM initialization failed\n");
}
#endif
