/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __SUN252I_CLOCK_H
#define __SUN252I_CLOCK_H

#include <linux/bitops.h>

/* SMHC module clock: HOSC with M and power-of-two N dividers. */
#define CCM_MMC_CTRL_M(x) ((x) - 1)
#define CCM_MMC_CTRL_N(x) ((x) << 8)
#define CCM_MMC_CTRL_OSCM24 0
#define CCM_MMC_CTRL_PLL6 BIT(24)
#define CCM_MMC_CTRL_ENABLE BIT(31)
#define CCM_MMC_CTRL_OCLK_DLY(x) 0
#define CCM_MMC_CTRL_SCLK_DLY(x) 0

int sun252i_v861_peri400m_rate(unsigned int *rate);

#endif
