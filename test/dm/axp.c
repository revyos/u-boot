// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <dm.h>
#include <dm/test.h>
#include <errno.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <test/test.h>
#include <test/ut.h>

static int dm_test_axp333_voltage(struct unit_test_state *uts)
{
	static const struct {
		const char *name;
		u8 reg;
		u8 mask;
		u8 selector;
		int uV;
	} cases[] = {
		{ "axp333-test-dcdc1", 0x13, 0x7f, 0, 500000 },
		{ "axp333-test-dcdc1", 0x13, 0x7f, 70, 1200000 },
		{ "axp333-test-dcdc1", 0x13, 0x7f, 71, 1220000 },
		{ "axp333-test-dcdc1", 0x13, 0x7f, 87, 1540000 },
		{ "axp333-test-dcdc1", 0x13, 0x7f, 88, 1600000 },
		{ "axp333-test-dcdc1", 0x13, 0x7f, 106, 3400000 },
		{ "axp333-test-dcdc2", 0x14, 0x7f, 85, 1500000 },
		{ "axp333-test-dcdc2", 0x14, 0x7f, 102, 1840000 },
		{ "axp333-test-dcdc3", 0x15, 0x7f, 102, 1840000 },
		{ "axp333-test-dcdc3", 0x15, 0x7f, 103, 3100000 },
		{ "axp333-test-dcdc3", 0x15, 0x7f, 106, 3400000 },
		{ "axp333-test-aldo1", 0x16, 0x1f, 0, 500000 },
		{ "axp333-test-aldo1", 0x16, 0x1f, 30, 3500000 },
		{ "axp333-test-aldo2", 0x17, 0x1f, 13, 1800000 },
	};
	struct udevice *reg, *pmic;
	int i;

	ut_assertok(pmic_get("pmic@36", &pmic));
	ut_asserteq(0x4a, pmic_reg_read(pmic, 3));
	ut_assert(pmic_reg_read(pmic, 0x1a) & BIT(4));
	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		ut_assertok(regulator_get_by_platname(cases[i].name, &reg));
		ut_assertok(pmic_reg_write(pmic, cases[i].reg, 0xff));
		ut_assertok(regulator_set_value(reg, cases[i].uV));
		ut_asserteq(cases[i].selector | (0xff & ~cases[i].mask),
			    pmic_reg_read(pmic, cases[i].reg));
		ut_asserteq(cases[i].uV, regulator_get_value(reg));
	}

	return 0;
}
DM_TEST(dm_test_axp333_voltage, UTF_SCAN_FDT);

static int dm_test_axp333_invalid_voltage(struct unit_test_state *uts)
{
	static const int invalid[] = {
		499999, 500001, 1840001, 2000000, 3000000, 3099999, 3400001,
	};
	struct udevice *reg, *pmic;
	int i;

	ut_assertok(pmic_get("pmic@36", &pmic));
	ut_assertok(regulator_get_by_platname("axp333-test-dcdc3", &reg));
	ut_assertok(regulator_set_value(reg, 3300000));
	for (i = 0; i < ARRAY_SIZE(invalid); i++) {
		ut_asserteq(-EINVAL, regulator_set_value(reg, invalid[i]));
		ut_asserteq(3300000, regulator_get_value(reg));
	}
	ut_assertok(pmic_reg_write(pmic, 0x15, 0x7f));
	ut_asserteq(-EINVAL, regulator_get_value(reg));
	ut_assertok(regulator_get_by_platname("axp333-test-aldo1", &reg));
	ut_assertok(pmic_reg_write(pmic, 0x16, 0x1f));
	ut_asserteq(-EINVAL, regulator_get_value(reg));

	return 0;
}
DM_TEST(dm_test_axp333_invalid_voltage, UTF_SCAN_FDT);

static int dm_test_axp333_enable(struct unit_test_state *uts)
{
	static const char * const names[] = {
		"axp333-test-dcdc1", "axp333-test-dcdc2", "axp333-test-dcdc3",
		"axp333-test-aldo1", "axp333-test-aldo2",
	};
	struct udevice *reg, *pmic;
	int i;

	ut_assertok(pmic_get("pmic@36", &pmic));
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		ut_assertok(regulator_get_by_platname(names[i], &reg));
		ut_assertok(pmic_reg_write(pmic, 0x10, 0xff));
		ut_assertok(regulator_set_enable(reg, false));
		ut_asserteq(0xff & ~BIT(i), pmic_reg_read(pmic, 0x10));
		ut_asserteq(0, regulator_get_enable(reg));
		ut_assertok(regulator_set_enable(reg, true));
		ut_asserteq(0xff, pmic_reg_read(pmic, 0x10));
	}
	ut_assertok(regulator_get_by_platname("axp333-test-rtc", &reg));
	ut_asserteq(1800000, regulator_get_value(reg));
	ut_asserteq(1, regulator_get_enable(reg));
	ut_asserteq(-EOPNOTSUPP, regulator_set_enable(reg, false));

	return 0;
}
DM_TEST(dm_test_axp333_enable, UTF_SCAN_FDT);
