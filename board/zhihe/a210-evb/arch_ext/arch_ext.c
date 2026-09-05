/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <cpu_func.h>
#include <stdio.h>
#include <string.h>

#include "arch_ext.h"

extern ulong __bram_text_start__;
extern ulong __bram_data_end__;
extern ulong _image_binary_end;
#define BRAM_SECTION_SIZE ((size_t)((uchar*)&__bram_data_end__ - (uchar*)&__bram_text_start__))

/* u-boot/lib/fdtdec.c */
ulong *board_spl_get_separate_bss_binary_end(void)
{
	return (ulong *)((uchar*)&_image_binary_end + BRAM_SECTION_SIZE);
}

void spl_prepare_bram_section(void)
{
	memcpy(&__bram_text_start__, &_image_binary_end, BRAM_SECTION_SIZE);
	/* Publish the copied instructions before executing the BRAM trampoline. */
	flush_dcache_range((ulong)&__bram_text_start__,
			  ALIGN((ulong)&__bram_data_end__, CONFIG_SYS_CACHELINE_SIZE));
	invalidate_icache_all();
}

ATT_BRAM_TEXT void bram_main(void (*cb)(ulong), ulong cb_param)
{
	cb(cb_param);
}
