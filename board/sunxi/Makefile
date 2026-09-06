# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
#
# Based on some other board Makefile
#
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
ifdef CONFIG_MACH_SUN252I_V861
obj-y += sun252i_v861.o
else
obj-y	+= board.o
obj-$(CONFIG_SUN7I_GMAC)	+= gmac.o
obj-$(CONFIG_MACH_SUN4I)	+= dram_sun4i_auto.o
obj-$(CONFIG_MACH_SUN5I)	+= dram_sun5i_auto.o
obj-$(CONFIG_MACH_SUN7I)	+= dram_sun5i_auto.o
obj-$(CONFIG_$(PHASE_)CHIP_DIP_SCAN)	+= chip.o
endif
