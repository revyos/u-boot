// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <asm/cache.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <regmap.h>
#include <reset.h>
#include <syscon.h>
#include <wait_bit.h>
#include <linux/bitfield.h>
#include <linux/delay.h>

#include "dwc_eth_qos.h"

/* SYSCFG registers */
#define GMAC_SYSREG_OFS  0xF000
#define GMAC_CLKCTRL     (GMAC_SYSREG_OFS + 0x00)
#define GMAC_CTRL        (GMAC_SYSREG_OFS + 0x04)

/* SYSCFG value */
#define GMAC_CLKTRL_EN_ALL        0x1F
#define GMAC_CLKCTRL_SPEED_MASK  (0x3 << 8)
#define GMAC_CLKCTRL_1G          (0x0 << 8)
#define GMAC_CLKCTRL_10M         (0x2 << 8)
#define GMAC_CLKCTRL_100M        (0x3 << 8)

#define GMAC_CTRL_INTF_MASK 0xF
#define GMAC_CTRL_RGMII     0x1
#define GMAC_CTRL_RMII      0x4

/* Clocks resouce 
 * The following resource list corresponds to 
 * "aclk", "hclk", "x2h_aclk", and "x2h_hclk" respectively.
 */
#define CLK_RES_COUNT 4
#define CLK_RES_NAMES {"aclk", "hclk", "x2h_aclk", "x2h_hclk"}
#define CLK_RES_INIT(clks, eqos) do { \
	clks[0] = &eqos->clk_slave_bus; \
	clks[1] = &eqos->clk_master_bus; \
	clks[2] = &eqos->clk_rx; \
	clks[3] = &eqos->clk_tx; \
} while(0) 

static int zhihe_resets_deassert(struct udevice *dev)
{
	int i;
	int ret;
	struct reset_ctl resets[4];
	const char *reset_names[] = {"arst", "hrst", "x2h_arst", "x2h_hrst"};

	dev_dbg(dev, "%s\n", __func__);

	/* Check resets */
	for (i = 0; i < sizeof(resets) / sizeof(struct reset_ctl); i++) {
		ret = reset_get_by_name(dev, reset_names[i], &resets[i]);
		if (ret) {
			printf("Failed to get %s\n", reset_names[i]);
			goto err_resets;
		}
	}

	/* Deassert all resets */
	for (i = 0; i < sizeof(resets) / sizeof(struct reset_ctl); i++) {
		reset_deassert(&resets[i]);
	}
	return 0;

err_resets:
	for (int j = 0; j < i; j++) {
		reset_free(&resets[j]);
	}
	return -EINVAL;
}

static ulong eqos_get_tick_clk_rate_zhihe(struct udevice *dev)
{
	struct eqos_priv __maybe_unused *eqos = dev_get_priv(dev);

	/* hclk -> clk_master_bus */

	return clk_get_rate(&eqos->clk_master_bus);
}

static int eqos_start_clks_zhihe(struct udevice *dev)
{
	int iclk;
	int ret;
	struct eqos_priv __maybe_unused *eqos = dev_get_priv(dev);
	struct clk *clks[CLK_RES_COUNT];

	dev_dbg(dev, "%s:\n", __func__);

	/* Enable all clks */
	CLK_RES_INIT(clks, eqos);
	for(iclk = 0; iclk < CLK_RES_COUNT; iclk++) {
		ret = clk_enable(clks[iclk]);
		if (ret < 0) {
			dev_err(dev, "Failed to enable clk_master_bus\n");
			goto err_clks;
		}
	}
	ret = zhihe_resets_deassert(dev);
	if (ret < 0) {
		goto err_resets;
	}

	/* Enable gmac clkctrl */
	writel(GMAC_CLKTRL_EN_ALL, (void __iomem *)(eqos->regs + GMAC_CLKCTRL));

	dev_dbg(dev, "%s: OK\n", __func__);
	return 0;

err_clks:
	for (int j = 0; j < iclk; j++) {
		clk_disable(clks[j]);
	}

err_resets:
	dev_dbg(dev, "%s: FAILED: %d\n", __func__, ret);
	return ret;
}

static int eqos_stop_clks_zhihe(struct udevice *dev)
{
	struct clk *clks[CLK_RES_COUNT];
	struct eqos_priv __maybe_unused *eqos = dev_get_priv(dev);

	dev_dbg(dev, "%s:\n", __func__);

	/* Disable all clks */
	CLK_RES_INIT(clks, eqos);
	for (int i = 0; i < CLK_RES_COUNT; i++) {
		clk_disable(clks[i]);
	}

	return 0;
}

static int eqos_set_tx_clk_speed_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	int speed = eqos->phy->speed;
	unsigned int reg = 0;
	phy_interface_t interface = eqos->config->interface(dev);

	dev_dbg(dev, "%s: speed %d, phyif %d\n", __func__, speed, interface);

	reg = readl((void __iomem*)(eqos->regs + GMAC_CLKCTRL));
	switch(speed) {
	case SPEED_10:
		reg &= ~GMAC_CLKCTRL_SPEED_MASK;
		reg |= GMAC_CLKCTRL_10M;
		break;
	case SPEED_100:
		reg &= ~GMAC_CLKCTRL_SPEED_MASK;
		reg |= GMAC_CLKCTRL_100M;
		break;
	case SPEED_1000:
		reg &= ~GMAC_CLKCTRL_SPEED_MASK;
		reg |= GMAC_CLKCTRL_1G;
		break;
	default:
		dev_err(dev, "unsupported speed: %d\n", speed);
		return -EINVAL;
	}
	writel(reg, (void __iomem*)(eqos->regs + GMAC_CLKCTRL));

	/* Configure phy interface */
	reg = readl((void __iomem*)(eqos->regs + GMAC_CTRL));
	switch(interface) {
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
		reg &= ~GMAC_CTRL_INTF_MASK;
		reg |= GMAC_CTRL_RGMII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		reg &= ~GMAC_CTRL_INTF_MASK;
		reg |= GMAC_CTRL_RMII;
		break;
	default:
		dev_err(dev, "unsupported phy interface: %d\n", interface);
		return -EINVAL;
	}
	writel(reg, (void __iomem*)(eqos->regs + GMAC_CTRL));

	return 0;
}

static int eqos_start_resets_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	dev_dbg(dev, "%s\n", __func__);

	if (dm_gpio_is_valid(&eqos->phy_reset_gpio)) {
		dm_gpio_set_value(&eqos->phy_reset_gpio, 1);
		/* At least 10ms in databook 6.5 Reset */
		mdelay(20);
		dm_gpio_set_value(&eqos->phy_reset_gpio, 0);
	}
	return 0;
}

static int eqos_stop_resets_zhihe(struct udevice *dev)
{
	//struct eqos_priv *eqos = dev_get_priv(dev);

	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

static int eqos_probe_resources_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;
	struct clk *clks[CLK_RES_COUNT];
	const char *clk_names[] = CLK_RES_NAMES;

	/* Get all clk handle */
	CLK_RES_INIT(clks, eqos);
	for(int i = 0; i < CLK_RES_COUNT; i++) {
		ret = clk_get_by_name(dev, clk_names[i], clks[i]);
		if (ret) {
			dev_err(dev, "Failed to get %s\n", clk_names[i]);
			goto err_probe;
		}
	}

	/* Get reset gpio pin (optional) */
	ret = gpio_request_by_name(dev, "phy-reset-gpios", 0,
				   &eqos->phy_reset_gpio, GPIOD_IS_OUT);
	if (ret)
		pr_warn("No phy reset gpio provided: %d\n", ret);

	dev_dbg(dev, "%s: OK\n", __func__);
	return 0;

err_probe:
	dev_dbg(dev, "%s: returns %d\n", __func__, ret);

	return ret;
}

static int eqos_remove_resources_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	dev_dbg(dev, "%s:\n", __func__);

	if (dm_gpio_is_valid(&eqos->phy_reset_gpio))
		dm_gpio_free(dev, &eqos->phy_reset_gpio);

	return 0;
}

static struct eqos_ops eqos_zhihe_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_zhihe,
	.eqos_remove_resources = eqos_remove_resources_zhihe,
	.eqos_stop_resets = eqos_stop_resets_zhihe,
	.eqos_start_resets = eqos_start_resets_zhihe,
	.eqos_stop_clks = eqos_stop_clks_zhihe,
	.eqos_start_clks = eqos_start_clks_zhihe,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_zhihe,
	.eqos_get_enetaddr = eqos_null_ops,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_zhihe
};

struct eqos_config __maybe_unused eqos_zhihe_a2xx_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10000,
	.swr_wait = 50,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
	.axi_bus_width = EQOS_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &eqos_zhihe_ops
};
