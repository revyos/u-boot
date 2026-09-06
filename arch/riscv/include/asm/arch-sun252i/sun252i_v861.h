/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __ASM_ARCH_SUN252I_V861_H
#define __ASM_ARCH_SUN252I_V861_H

#define SUN252I_V861_CCU_BASE	0x02001000
#define SUN252I_V861_UART0_BASE	0x02500000

void sun252i_v861_cpu_init(void);
void sun252i_v861_uart_init(void);
unsigned long sunxi_dram_init(void);
void sun252i_v861_i2c2_init(void);
void sun252i_v861_spif_init(void);
void sun252i_v861_spif_disable(void);
int sun252i_v861_spif_set_clock(unsigned int speed);

#endif
