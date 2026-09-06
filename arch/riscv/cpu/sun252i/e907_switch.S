/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2026 Han Gao <gaohan@iscas.ac.cn>
 *
 * Stackless E907 to C907 handoff. Like the ARM sunxi RMR switch, this
 * reference is not built by U-Boot. Its RV32 instruction words are embedded
 * in arch/riscv/include/asm/arch-sun252i/boot0.h.
 *
 * Regenerate with an upstream RISC-V toolchain:
 * ${CROSS_COMPILE}gcc -c -march=rv32i_zicsr -mabi=ilp32 \
 *     -o e907_switch.o arch/riscv/cpu/sun252i/e907_switch.S
 * ${CROSS_COMPILE}ld -m elf32lriscv --no-relax -Ttext=0x100060 \
 *     -e sun252i_v861_e907_start -o e907_switch.elf e907_switch.o
 * ${CROSS_COMPILE}objdump -d e907_switch.elf
 * Add -DCONFIG_ARCH_RV64I to the assembler command for a C907 RV64 target.
 * The E907 handoff itself must always be assembled with the RV32 options.
 *
 * Keep the final entry-address word symbolic in boot0.h so the SPL linker
 * resolves it. The E907 must never use the C907 stack or global data.
 */

	.option norvc
	.option norelax
	.text
	.globl sun252i_v861_e907_start

	.macro update address, clear, set
	li	t0, \address
	lw	t1, 0(t0)
	li	t2, ~(\clear)
	and	t1, t1, t2
	li	t2, \set
	or	t1, t1, t2
	sw	t1, 0(t0)
	.endm

	.macro reset bit
	li	t0, 0x02001504
	lw	t1, 0(t0)
	li	t2, 0x16aa0000
	or	t1, t1, t2
	li	t2, ~(1 << \bit)
	and	t2, t1, t2
	sw	t2, 0(t0)
	li	t2, 1 << \bit
	or	t1, t1, t2
	sw	t1, 0(t0)
	.endm

sun252i_v861_e907_start:
	/* Disable machine interrupts while resetting the application cluster. */
	csrci	mstatus, 8

	/* Preserve the established PLL setup and settling interval. */
	li	t0, 0x02001020
	li	t1, 0xf8216310
	sw	t1, 0(t0)
	li	t1, 200
1:
	nop
	addi	t1, t1, -1
	bnez	t1, 1b

	/* Clock the PLIC from PERI_200M and reset APB, PLIC and IPMC. */
	update 0x02001508, 0x01000000, 0x80000000
	reset 2
	reset 3
	update 0x0200150c, 0x10000, 0
	update 0x0200150c, 0, 0x10001
	reset 12

	/* Select the C907 execution width, independently of the E907 ISA. */
#ifdef CONFIG_ARCH_RV64I
	update 0x08008108, 0x300, 0x200
#else
	update 0x08008108, 0x300, 0x100
#endif
	li	t0, 0x08008100
.Lload_entry:
	auipc	t1, %pcrel_hi(.Lentry)
	lw	t1, %pcrel_lo(.Lload_entry)(t1)
	sw	t1, 0(t0)
	sw	zero, 4(t0)

	/* Enable the cluster clock and release the cluster and both cores. */
	update 0x02001500, 0x30000, 0
	reset 0
	reset 1
	reset 8
	reset 4
	reset 5
	fence	iorw, iorw

	/* Leave the E907 in light sleep. */
	csrci	0x7e1, 12
	csrsi	0x7e1, 4
1:
	wfi
	j	1b

.Lentry:
	.word	sun252i_v861_c907_start
	.org	sun252i_v861_e907_start + 0x420
sun252i_v861_c907_start:
