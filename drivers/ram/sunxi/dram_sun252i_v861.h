/* SPDX-License-Identifier: GPL-2.0+ */
/* Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn> */

/*
 * V861 DDR IO resistor calibration.
 * The PHY uses the SID calibration byte, or GPADC channel 4 when absent.
 */
static int sun252i_v861_res_calibrate(u32 *ddr_code)
{
	static const u8 external_codes[] = {
		3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 10, 10, 11, 12, 12, 13
	};
	u32 adc_gate = readl(0x020019ec);
	u32 ths_gate = readl(0x020019fc);
	u32 mode, sum, sample, code, timeout;
	int ret = 0;

	/* Reset the ADC clock domain before configuring acquisition. */
	clrbits_le32(0x02001f04, 0x5U << 20);
	setbits_le32(0x02001f04, 0x5U << 20);
	writel(adc_gate & ~BIT(16), 0x020019ec);
	udelay(2);
	writel(adc_gate | BIT(16), 0x020019ec);
	writel(adc_gate | BIT(16) | BIT(0), 0x020019ec);

	setbits_le32(0x02001e0c, BIT(2));
	writel(0x00500000, 0x02001f04);
	writel(adc_gate | BIT(16) | BIT(0), 0x020019ec);
	writel(ths_gate | BIT(16) | BIT(0), 0x020019fc);
	writel(BIT(16), 0x02009404);
	udelay(50);
	writel(0x01df0164, 0x02009000);
	clrsetbits_le32(0x02009004, 3 << 18, (2 << 18) | BIT(23));
	setbits_le32(0x02009004, BIT(16));
	setbits_le32(0x02009008, BIT(4));
	udelay(50);
	setbits_le32(0x02009028, BIT(4));

	for (mode = 1; mode <= 3; mode += 2) {
		writel(0, 0x03000160);
		/* Select the source before enabling resistor calibration. */
		writel(mode << 2, 0x03000160);
		setbits_le32(0x03000160, BIT(1));
		sum = 0;
		for (sample = 0; sample < 10; sample++) {
			writel(BIT(4), 0x02009038);
			timeout = 10000;
			while (!(readl(0x02009038) & BIT(4))) {
				if (!--timeout) {
					printf("DRAM: resistor ADC timeout\n");
					ret = -1;
					goto out;
				}
				udelay(1);
			}
			sum += readl(0x02009090);
		}
		sum /= 10;
		/* Do not silently substitute code zero for an invalid sample. */
		if (sum < 1870 || sum >= 1870 + 16 * 21) {
			printf("DRAM: resistor ADC out of range: %u\n", sum);
			ret = -1;
			goto out;
		}
		code = (sum - 1870) / 21;
		if (mode == 3)
			code = external_codes[code];
		/* The DDR hook does not reprogram USB or CSI PHY calibration. */
		if (mode == 3) {
			writel(0x19180000 | code, 0x03000164);
			*ddr_code = code;
		}
	}
out:
	writel(0, 0x03000160);
	writel(adc_gate, 0x020019ec);
	writel(ths_gate, 0x020019fc);
	return ret;
}

static int sun252i_v861_zq_init(u32 tpr13)
{
	u32 code;

	if (tpr13 & BIT(16)) {
		writel(0, 0x03000160);
		writel(0x19210000, 0x03000164);
		udelay(10);
		return 0;
	}
	writel(0, 0x07010254);
	code = (readl(0x03006218) >> 8) & 0xff;
	if (!code) {
		clrbits_le32(0x03000160, BIT(1));
		udelay(10);
		if (sun252i_v861_res_calibrate(&code))
			return -1;
	}
	writel(0x19180000 | (code << 8) | code, 0x03000164);
	udelay(10);
	return 0;
}

/* Tables and selection follow the V861 vendor library's AC/DQ maps. */
static void sun252i_v861_phy_remap(u32 tpr13)
{
	static const u8 ac[4][22] = {
		{3, 4, 9, 6, 12, 14, 13, 5, 10, 16, 21, 17, 18, 8, 19, 1,
		 2, 22, 15, 11, 7, 20},
		{13, 4, 5, 1, 2, 6, 7, 3, 10, 8, 19, 9, 12, 0, 0, 0,
		 11, 22, 17, 18, 21, 20},
		{0, 18, 6, 13, 3, 5, 8, 2, 10, 7, 19, 9, 12, 0, 0, 0,
		 11, 22, 17, 4, 21, 20},
		{5, 19, 2, 4, 7, 1, 12, 6, 10, 3, 17, 9, 8, 14, 0, 0,
		 11, 21, 22, 13, 18, 20},
	};
	static const u8 dq[4][18] = {
		{4, 6, 5, 1, 2, 7, 0, 3, 8, 0, 7, 4, 3, 5, 2, 1, 6, 8},
		{6, 2, 4, 1, 3, 7, 0, 5, 8, 2, 6, 0, 5, 1, 8, 4, 3, 7},
		{7, 2, 4, 1, 3, 6, 0, 5, 8, 2, 7, 0, 5, 1, 8, 6, 3, 4},
		{3, 2, 6, 7, 5, 4, 0, 1, 8, 5, 1, 4, 0, 3, 7, 2, 6, 8},
	};
	const u8 *a = ac[(tpr13 >> 18) & 3];
	const u8 *d = dq[(tpr13 >> 18) & 3];
	u32 value, i;

	value = 0;
	for (i = 0; i < 5; i++)
		value |= (u32)a[i] << (5 * (i + 1));
	writel(value, 0x03102500);
	value = 0;
	for (i = 0; i < 6; i++)
		value |= (u32)a[5 + i] << (5 * i);
	writel(value, 0x03102504);
	value = 0;
	for (i = 0; i < 5; i++)
		value |= (u32)a[11 + i] << (5 * i);
	writel(value, 0x03102508);
	value = 0;
	for (i = 0; i < 6; i++)
		value |= (u32)a[16 + i] << (5 * i);
	writel(value, 0x0310250c);
	setbits_le32(0x03102500, BIT(0));
	if (((tpr13 >> 18) & 3) && !(tpr13 & BIT(10)))
		setbits_le32(0x03103208, BIT(12));
	udelay(1000);
	for (u32 lane = 0; lane < 2; lane++) {
		value = 0;
		for (i = 0; i < 8; i++)
			value |= (u32)d[lane * 9 + i] << (4 * i);
		writel(value, 0x03102510 + lane * 8);
		writel(d[lane * 9 + 8], 0x03102514 + lane * 8);
	}
	setbits_le32(0x03102500, BIT(1) | BIT(2));
}
