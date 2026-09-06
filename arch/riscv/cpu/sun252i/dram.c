// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <asm/global_data.h>
#include <fdtdec.h>
#include <init.h>
#include <linux/errno.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	int ret;

	ret = fdtdec_setup_mem_size_base();
	if (ret)
		return ret;

	return gd->ram_size ? 0 : -EINVAL;
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}
