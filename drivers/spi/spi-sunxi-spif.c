// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <dm.h>
#include <spi.h>
#include <spi-mem.h>
#include <asm/arch/sun252i_v861.h>
#include <asm/barrier.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>

#define SPIF_VERSION		0x00
#define SPIF_GCR			0x04
#define SPIF_GAR			0x08
#define SPIF_TCR			0x0c
#define SPIF_ISR			0x18
#define SPIF_CSD			0x1c
#define SPIF_DMA_CTL		0x40
#define SPIF_DMA_DESC		0x44
#define SPIF_DMA_MODE		BIT(0)
#define SPIF_CS_ACTIVE_LOW	BIT(8)
#define SPIF_FIFO_RESET		GENMASK(1, 0)
#define SPIF_SOFT_RESET		BIT(3)
#define SPIF_DMA_RESET		BIT(4)
#define SPIF_DMA_DONE		BIT(24)
#define SPIF_PHASE_RX		BIT(8)
#define SPIF_PHASE_TX		BIT(12)
#define SPIF_PHASE_DUMMY		BIT(16)
#define SPIF_PHASE_ADDR		BIT(24)
#define SPIF_PHASE_CMD		BIT(28)
#define SPIF_DESC_LAST		BIT(0)
#define SPIF_DESC_READ		BIT(1)
#define SPIF_DESC_BURST16		(7U << 4)
#define SPIF_DESC_BLOCK64		(3U << 24)
#define SPIF_DESC_NORMAL		BIT(28)
#define SPIF_DMA_START		BIT(0)
#define SPIF_DMA_DESC_LEN		(32U << 4)
#define SPIF_TIMEOUT_US		100000
#define SPIF_BUFFER_SIZE		4096

struct sunxi_spif_desc {
	u32 burst;
	u32 block_len;
	u32 data_addr;
	u32 next_desc;
	u32 phase;
	u32 flash_addr;
	u32 bus_width;
	u32 transfer_len;
} __aligned(ARCH_DMA_MINALIGN);

struct sunxi_spif_priv {
	void __iomem *base;
	u32 mode;
	bool failed;
	struct sunxi_spif_desc desc;
	u8 buffer[SPIF_BUFFER_SIZE] __aligned(ARCH_DMA_MINALIGN);
};

static int sunxi_spif_reset(struct sunxi_spif_priv *priv)
{
	u32 value;
	int ret;

	setbits_le32(priv->base + SPIF_GAR, SPIF_DMA_RESET);
	ret = readl_poll_timeout(priv->base + SPIF_GAR, value,
				 !(value & SPIF_DMA_RESET), SPIF_TIMEOUT_US);
	if (ret)
		return ret;
	setbits_le32(priv->base + SPIF_GAR, SPIF_SOFT_RESET);
	return readl_poll_timeout(priv->base + SPIF_GAR, value,
				  !(value & SPIF_SOFT_RESET), SPIF_TIMEOUT_US);
}

static bool sunxi_spif_supports_op(struct spi_slave *slave,
				   const struct spi_mem_op *op)
{
	if (op->cmd.nbytes != 1 || op->cmd.buswidth != 1 ||
	    op->addr.nbytes > 4 || op->addr.val > U32_MAX ||
	    (op->addr.nbytes && op->addr.buswidth != 1) ||
	    (op->dummy.nbytes && op->dummy.buswidth != 1) ||
	    op->dummy.nbytes > 31 ||
	    (op->data.nbytes && op->data.buswidth != 1))
		return false;

	return spi_mem_default_supports_op(slave, op);
}

static int sunxi_spif_adjust_op_size(struct spi_slave *slave,
				     struct spi_mem_op *op)
{
	op->data.nbytes = min_t(unsigned int, op->data.nbytes, SPIF_BUFFER_SIZE);
	return 0;
}

static int sunxi_spif_exec_op(struct spi_slave *slave,
			      const struct spi_mem_op *op)
{
	struct sunxi_spif_priv *priv = dev_get_priv(slave->dev->parent);
	struct sunxi_spif_desc *desc = &priv->desc;
	dma_addr_t buffer = 0, descriptor;
	enum dma_data_direction direction = op->data.dir == SPI_MEM_DATA_IN ?
		DMA_FROM_DEVICE : DMA_TO_DEVICE;
	u32 value;
	int ret;

	if (priv->failed)
		return -EIO;
	if (!sunxi_spif_supports_op(slave, op) ||
	    op->data.nbytes > sizeof(priv->buffer))
		return -EOPNOTSUPP;

	memset(desc, 0, sizeof(*desc));
	desc->burst = SPIF_DESC_LAST | SPIF_DESC_BURST16;
	desc->block_len = SPIF_DESC_BLOCK64 | op->data.nbytes;
	desc->phase = SPIF_PHASE_CMD;
	desc->bus_width = (u32)op->cmd.opcode << 24;
	desc->transfer_len = SPIF_DESC_NORMAL | op->data.nbytes;
	if (op->addr.nbytes) {
		desc->phase |= SPIF_PHASE_ADDR;
		desc->flash_addr = op->addr.val;
		desc->transfer_len |= (op->addr.nbytes - 1) << 24;
	}
	if (op->dummy.nbytes) {
		desc->phase |= SPIF_PHASE_DUMMY;
		desc->transfer_len |= (op->dummy.nbytes * 8) << 16;
	}
	if (op->data.nbytes) {
		if (op->data.dir == SPI_MEM_DATA_IN) {
			desc->phase |= SPIF_PHASE_RX;
			desc->burst |= SPIF_DESC_READ;
		} else {
			desc->phase |= SPIF_PHASE_TX;
			memcpy(priv->buffer, op->data.buf.out, op->data.nbytes);
		}
		buffer = dma_map_single(priv->buffer, op->data.nbytes, direction);
		desc->data_addr = buffer >> 2;
	}

	/* A private aligned buffer also protects short and unaligned reads. */
	descriptor = dma_map_single(desc, sizeof(*desc), DMA_TO_DEVICE);
	/* Publish the descriptor and payload before starting the DMA engine. */
	mb();

	setbits_le32(priv->base + SPIF_GAR, SPIF_FIFO_RESET);
	writel(SPIF_CS_ACTIVE_LOW | SPIF_DMA_MODE | priv->mode,
	       priv->base + SPIF_GCR);
	writel(SPIF_DMA_DONE, priv->base + SPIF_ISR);
	writel(descriptor >> 2, priv->base + SPIF_DMA_DESC);
	writel(SPIF_DMA_DESC_LEN | SPIF_DMA_START, priv->base + SPIF_DMA_CTL);
	ret = readl_poll_timeout(priv->base + SPIF_ISR, value,
				 value & SPIF_DMA_DONE, SPIF_TIMEOUT_US);
	if (ret) {
		/* Abort DMA before the caller can reuse the operation buffer. */
		if (sunxi_spif_reset(priv)) {
			sun252i_v861_spif_disable();
			priv->failed = true;
		}
		goto unmap;
	}
	writel(SPIF_DMA_DONE, priv->base + SPIF_ISR);

unmap:
	/* Observe DMA completion or reset before returning buffer ownership. */
	mb();
	dma_unmap_single(descriptor, sizeof(*desc), DMA_TO_DEVICE);
	if (op->data.nbytes)
		dma_unmap_single(buffer, op->data.nbytes, direction);
	if (!ret && op->data.nbytes && op->data.dir == SPI_MEM_DATA_IN)
		memcpy(op->data.buf.in, priv->buffer, op->data.nbytes);
	return ret;
}

static int sunxi_spif_set_speed(struct udevice *bus, uint speed)
{
	struct sunxi_spif_priv *priv = dev_get_priv(bus);

	if (priv->failed)
		return -EIO;
	return sun252i_v861_spif_set_clock(speed);
}

static int sunxi_spif_set_mode(struct udevice *bus, uint mode)
{
	struct sunxi_spif_priv *priv = dev_get_priv(bus);

	if (mode & ~(SPI_CPHA | SPI_CPOL))
		return -EINVAL;
	priv->mode = (mode & SPI_CPHA ? BIT(4) : 0) |
		     (mode & SPI_CPOL ? BIT(5) : 0);
	return 0;
}

static int sunxi_spif_claim_bus(struct udevice *dev)
{
	/* Only the board's CS0 is wired to the flash controller. */
	return spi_chip_select(dev) ? -ENODEV : 0;
}

static int sunxi_spif_probe(struct udevice *dev)
{
	struct sunxi_spif_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base || priv->base == (void *)FDT_ADDR_T_NONE)
		return -EINVAL;
	sun252i_v861_spif_init();
	ret = sunxi_spif_reset(priv);
	if (ret)
		return ret;
	if (readl(priv->base + SPIF_VERSION) < 0x10002)
		return -ENODEV;
	writel(0, priv->base + SPIF_TCR);
	writel((5 << 16) | (6 << 8) | 6, priv->base + SPIF_CSD);
	return 0;
}

static const struct spi_controller_mem_ops sunxi_spif_mem_ops = {
	.supports_op = sunxi_spif_supports_op,
	.adjust_op_size = sunxi_spif_adjust_op_size,
	.exec_op = sunxi_spif_exec_op,
};

static const struct dm_spi_ops sunxi_spif_ops = {
	.claim_bus = sunxi_spif_claim_bus,
	.set_speed = sunxi_spif_set_speed,
	.set_mode = sunxi_spif_set_mode,
	.mem_ops = &sunxi_spif_mem_ops,
};

static const struct udevice_id sunxi_spif_ids[] = {
	{ .compatible = "allwinner,sun252i-v861-spif" },
	{ }
};

U_BOOT_DRIVER(sunxi_spif) = {
	.name = "sunxi_spif",
	.id = UCLASS_SPI,
	.of_match = sunxi_spif_ids,
	.probe = sunxi_spif_probe,
	.priv_auto = sizeof(struct sunxi_spif_priv),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
	.ops = &sunxi_spif_ops,
};
