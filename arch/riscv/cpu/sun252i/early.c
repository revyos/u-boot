// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 */

#include <asm/arch/sun252i_v861.h>
#include <asm/io.h>
#include <ns16550.h>
#include <sunxi_gpio.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <asm/arch/clock.h>

#define SUN252I_V861_UART_BGR	(SUN252I_V861_CCU_BASE + 0x90c)
#define SUN252I_V861_BGR_RESET	BIT(16)
#define SUN252I_V861_BGR_GATE	BIT(0)

#define SUN252I_V861_PLL_ENABLE	BIT(31)
#define SUN252I_V861_PLL_LDO_ENABLE	BIT(30)
#define SUN252I_V861_PLL_LOCK_ENABLE	BIT(29)
#define SUN252I_V861_PLL_LOCK	BIT(28)
#define SUN252I_V861_PLL_OUTPUT	BIT(27)
#define SUN252I_V861_PLL_N		GENMASK(15, 8)
#define SUN252I_V861_PLL_P0		GENMASK(18, 16)

#define SUN252I_V861_SPIF_CLK_ENABLE	BIT(31)
#define SUN252I_V861_SPIF_CLK_SOURCE	GENMASK(26, 24)
#define SUN252I_V861_SPIF_CLK_N	GENMASK(9, 8)
#define SUN252I_V861_SPIF_CLK_M	GENMASK(3, 0)

void sun252i_v861_spif_disable(void)
{
	clrbits_le32((void *)(SUN252I_V861_CCU_BASE + 0x950), BIT(31));
}

int sun252i_v861_spif_set_clock(unsigned int speed)
{
	void __iomem *clock = (void *)(SUN252I_V861_CCU_BASE + 0x950);
	unsigned int parent = 24000000, source = 0, div, n = 0;
	int ret;

	if (!speed)
		return -EINVAL;
	speed = min(speed, 50000000U);
	if (speed > parent) {
		ret = sun252i_v861_peri400m_rate(&parent);
		if (ret)
			return ret;
		source = 1;
	}
	div = DIV_ROUND_UP(parent, speed);
	while (div > 16 && n < 3) {
		n++;
		div = DIV_ROUND_UP(parent, speed * (1U << n));
	}
	if (div > 16)
		return -EINVAL;
	writel(0, clock);
	writel(SUN252I_V861_SPIF_CLK_ENABLE |
	       FIELD_PREP(SUN252I_V861_SPIF_CLK_SOURCE, source) |
	       FIELD_PREP(SUN252I_V861_SPIF_CLK_N, n) |
	       FIELD_PREP(SUN252I_V861_SPIF_CLK_M, div - 1), clock);
	return 0;
}

void sun252i_v861_spif_init(void)
{
	void __iomem *bgr = (void *)(SUN252I_V861_CCU_BASE + 0x96c);
	unsigned int pin;

	for (pin = SUNXI_GPC(0); pin <= SUNXI_GPC(5); pin++)
		sunxi_gpio_set_cfgpin(pin, SUN252I_V861_GPC_SPIF);
	sunxi_gpio_set_pull(SUNXI_GPC(1), SUNXI_GPIO_PULL_UP);
	sunxi_gpio_set_pull(SUNXI_GPC(4), SUNXI_GPIO_PULL_UP);
	sunxi_gpio_set_pull(SUNXI_GPC(5), SUNXI_GPIO_PULL_UP);
	sun252i_v861_spif_set_clock(24000000);
	clrbits_le32(bgr, BIT(20));
	setbits_le32(bgr, BIT(20) | BIT(4));
}

int sun252i_v861_peri400m_rate(unsigned int *rate)
{
	void __iomem *pll = (void __iomem *)(SUN252I_V861_CCU_BASE + 0x20);
	u32 value = readl(pll);
	unsigned int timeout = 1000;

	/* Keep the shared PLL divisors established by the boot chain. */
	writel(value | SUN252I_V861_PLL_LDO_ENABLE | SUN252I_V861_PLL_LOCK_ENABLE |
	       SUN252I_V861_PLL_ENABLE, pll);
	while (!(readl(pll) & SUN252I_V861_PLL_LOCK)) {
		if (!--timeout)
			return -ETIMEDOUT;
		udelay(1);
	}
	udelay(20);
	setbits_le32(pll, SUN252I_V861_PLL_OUTPUT);
	value = readl(pll);
	/* PERI_400M = HOSC * (N + 1) / (P0 + 1) / 3. */
	*rate = 8000000U * (FIELD_GET(SUN252I_V861_PLL_N, value) + 1);
	*rate /= FIELD_GET(SUN252I_V861_PLL_P0, value) + 1;
	return 0;
}

void sun252i_v861_cpu_init(void)
{
	unsigned long value;

	/* Invalidate local caches, BTB and BHT before enabling coherency. */
	value = 0x70013;
	__asm__ volatile ("csrw 0x7c2, %0" : : "r"(value) : "memory");
	value = BIT(0);
	__asm__ volatile ("csrw 0x7f3, %0" : : "r"(value) : "memory");

	/* Enable the T-Head implementation instructions used by the C907. */
	value = BIT(22);
	__asm__ volatile ("csrs 0x7c0, %0" : : "r"(value) : "memory");
	__asm__ volatile ("fence.i" : : : "memory");
}

void sun252i_v861_uart_init(void)
{
	void __iomem *bgr = (void *)SUN252I_V861_UART_BGR;
	void __iomem *uart = (void *)SUN252I_V861_UART0_BASE;
	u32 divisor = DIV_ROUND_CLOSEST(24000000, 16 * CONFIG_BAUDRATE);

	/* Reset UART0, then enable its APB gate. */
	clrbits_le32(bgr, SUN252I_V861_BGR_RESET);
	udelay(10);
	setbits_le32(bgr, SUN252I_V861_BGR_RESET | SUN252I_V861_BGR_GATE);

	/* Start the baud clock before the serial driver waits for TX empty. */
	writel(UART_LCR_BKSE | UART_LCR_WLS_8, uart + 0x0c);
	writel(divisor & 0xff, uart);
	writel(divisor >> 8, uart + 0x04);
	writel(UART_LCR_WLS_8, uart + 0x0c);
	writel(UART_FCR_FIFO_EN | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT,
	       uart + 0x08);
}

void sun252i_v861_i2c2_init(void)
{
	void __iomem *bgr = (void *)(SUN252I_V861_CCU_BASE + 0x91c);

	/* Release the retained PL pin state before selecting TWI2. */
	if (!(readl((void *)0x07090238) & BIT(0)))
		writel(BIT(0) | BIT(1) | BIT(2) | BIT(5), (void *)0x07090240);
	/* PL belongs to the separate always-on PIO bank. */
	sunxi_gpio_set_cfgpin(SUNXI_GPL(3), SUN252I_V861_GPL_TWI2);
	sunxi_gpio_set_cfgpin(SUNXI_GPL(4), SUN252I_V861_GPL_TWI2);
	sunxi_gpio_set_pull(SUNXI_GPL(3), SUNXI_GPIO_PULL_UP);
	sunxi_gpio_set_pull(SUNXI_GPL(4), SUNXI_GPIO_PULL_UP);
	clrbits_le32(bgr, BIT(18));
	udelay(10);
	setbits_le32(bgr, BIT(18) | BIT(2));
}
