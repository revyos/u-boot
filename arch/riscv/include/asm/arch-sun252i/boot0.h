/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Stackless E907 to C907 handoff, encoded for RV32I with Zicsr.
 * See arch/riscv/cpu/sun252i/e907_switch.S for the reference source
 * and instructions for regenerating these words.
 */

	.word	0x30047073	/* csrci mstatus,8 */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x02028293	/* addi t0,t0,32 */
	.word	0xf8216337	/* lui t1,0xf8216 */
	.word	0x31030313	/* addi t1,t1,784 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x0c800313	/* li t1,200 */
	.word	0x00000013	/* nop */
	.word	0xfff30313	/* addi t1,t1,-1 */
	.word	0xfe031ce3	/* bnez t1,(local) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50828293	/* addi t0,t0,1288 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0xff0003b7	/* lui t2,0xff000 */
	.word	0xfff38393	/* addi t2,t2,-1 */
	.word	0x00737333	/* and t1,t1,t2 */
	.word	0x800003b7	/* lui t2,0x80000 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xffb00393	/* li t2,-5 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x00400393	/* li t2,4 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xff700393	/* li t2,-9 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x00800393	/* li t2,8 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50c28293	/* addi t0,t0,1292 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0xffff03b7	/* lui t2,0xffff0 */
	.word	0xfff38393	/* addi t2,t2,-1 */
	.word	0x00737333	/* and t1,t1,t2 */
	.word	0x00000393	/* li t2,0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50c28293	/* addi t0,t0,1292 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0xfff00393	/* li t2,-1 */
	.word	0x00737333	/* and t1,t1,t2 */
	.word	0x000103b7	/* lui t2,0x10 */
	.word	0x00138393	/* addi t2,t2,1 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xfffff3b7	/* lui t2,0xfffff */
	.word	0xfff38393	/* addi t2,t2,-1 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x000013b7	/* lui t2,0x1 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x080082b7	/* lui t0,0x8008 */
	.word	0x10828293	/* addi t0,t0,264 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0xcff00393	/* li t2,-769 */
	.word	0x00737333	/* and t1,t1,t2 */
#ifdef CONFIG_ARCH_RV64I
	.word	0x20000393	/* li t2,512: start C907 in RV64 */
#else
	.word	0x10000393	/* li t2,256: start C907 in RV32 */
#endif
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x080082b7	/* lui t0,0x8008 */
	.word	0x10028293	/* addi t0,t0,256 */
	.word	0x00000317	/* auipc t1,0x0 */
	.word	0x12432303	/* lw t1,292(t1) */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x0002a223	/* sw zero,4(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50028293	/* addi t0,t0,1280 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0xfffd03b7	/* lui t2,0xfffd0 */
	.word	0xfff38393	/* addi t2,t2,-1 */
	.word	0x00737333	/* and t1,t1,t2 */
	.word	0x00000393	/* li t2,0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xffe00393	/* li t2,-2 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x00100393	/* li t2,1 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xffd00393	/* li t2,-3 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x00200393	/* li t2,2 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xeff00393	/* li t2,-257 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x10000393	/* li t2,256 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xfef00393	/* li t2,-17 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x01000393	/* li t2,16 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x020012b7	/* lui t0,0x2001 */
	.word	0x50428293	/* addi t0,t0,1284 */
	.word	0x0002a303	/* lw t1,0(t0) */
	.word	0x16aa03b7	/* lui t2,0x16aa0 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0xfdf00393	/* li t2,-33 */
	.word	0x007373b3	/* and t2,t1,t2 */
	.word	0x0072a023	/* sw t2,0(t0) */
	.word	0x02000393	/* li t2,32 */
	.word	0x00736333	/* or t1,t1,t2 */
	.word	0x0062a023	/* sw t1,0(t0) */
	.word	0x0ff0000f	/* fence */
	.word	0x7e167073	/* csrci 0x7e1,12 */
	.word	0x7e126073	/* csrsi 0x7e1,4 */
	.word	0x10500073	/* wfi */
	.word	0xffdff06f	/* j (local) */
	.word	sun252i_v861_c907_start

	/* Keep the C907 entry at offset 0x480 in the eGON image. */
	.org	_start + 0x420
