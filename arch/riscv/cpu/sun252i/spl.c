// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <asm/arch/cpu.h>
#include <asm/cache.h>
#include <cpu_func.h>
#include <asm/global_data.h>
#include <asm/system.h>
#include <hang.h>
#include <debug_uart.h>
#include <init.h>
#include <fdt_support.h>
#include <spl.h>
#include <sunxi_gpio.h>
#include <timer.h>

DECLARE_GLOBAL_DATA_PTR;

static void gpio_init(void)
{
	/* UART0 is routed to PH9/PH10 on the Avaota F2. */
	sunxi_gpio_set_cfgpin(SUNXI_GPH(9), SUN252I_V861_GPH_UART0);
	sunxi_gpio_set_cfgpin(SUNXI_GPH(10), SUN252I_V861_GPH_UART0);
	sunxi_gpio_set_pull(SUNXI_GPH(9), SUNXI_GPIO_PULL_UP);
	sunxi_gpio_set_pull(SUNXI_GPH(10), SUNXI_GPIO_PULL_UP);
}

void board_init_f(ulong dummy)
{
	int ret;

	sun252i_v861_cpu_init();

	ret = spl_early_init();
	if (ret)
		hang();
	/* CPU enumeration binds the architectural timer on the boot hart. */
	ret = riscv_cpu_setup();
	if (ret)
		hang();
	ret = dm_timer_init();
	if (ret)
		hang();

	/* UART reset timing uses the initialized architectural timer. */
	gpio_init();
	sun252i_v861_uart_init();
	debug_uart_init();

	preloader_console_init();
	sunxi_board_init();
}

void spl_board_init(void)
{
	/* BSS is cleared after board_init_f(); initialize CBO state afterwards. */
	if (riscv_get_cbom_block_size() != ARCH_DMA_MINALIGN)
		panic("Invalid cache-block size for DMA\n");
	enable_caches();
}

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_SPI;
}

void spl_perform_board_fixups(struct spl_image_info *spl_image)
{
	void *fdt = spl_image_fdt_addr(spl_image);
	int ret;

	if (!fdt || !gd->ram_size)
		panic("Missing DRAM information for firmware handoff\n");
	ret = fdt_fixup_memory(fdt, gd->ram_base, gd->ram_size);
	if (ret)
		panic("Cannot update firmware memory node (%d)\n", ret);
}
