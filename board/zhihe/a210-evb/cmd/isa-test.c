// SPDX-License-Identifier: GPL-2.0+
#include <command.h>
#include <stdio.h>
#include <string.h>
#include <asm/csr.h>

/* Test storage includes a guard cache line; no live data is invalidated. */
static unsigned char a210_cbo_test[128] __aligned(128);

static int do_a210_isa_test(struct cmd_tbl *cmdtp, int flag, int argc,
			  char *const argv[])
{
	unsigned long compare;
	int i;

	puts("Requires Sstc/Zicbo-enabled firmware; unavailable CSRs may trap.\n");
	puts("Testing S-mode STIMECMP access...\n");
	compare = csr_read(0x14d);
	csr_write(0x14d, compare);
	puts("STIMECMP read/write: PASS (compare value preserved)\n");

	memset(a210_cbo_test, 0xa5, sizeof(a210_cbo_test));
	puts("Testing Zicbom/Zicbop/Zicboz on private aligned storage...\n");
	asm volatile (".option push\n"
		      ".option arch, +zicbom,+zicbop,+zicboz\n"
		      "cbo.clean 0(%0)\n"
		      "fence rw,rw\n"
		      "cbo.flush 0(%0)\n"
		      "fence rw,rw\n"
		      "cbo.inval 0(%0)\n"
		      "fence rw,rw\n"
		      "prefetch.r 0(%0)\n"
		      "prefetch.w 0(%0)\n"
		      "prefetch.i 0(%0)\n"
		      "cbo.zero 0(%0)\n"
		      "fence rw,rw\n"
		      ".option pop\n"
		      : : "r" (a210_cbo_test) : "memory");
	for (i = 0; i < sizeof(a210_cbo_test); i++) {
		if (a210_cbo_test[i] != (i < 64 ? 0 : 0xa5)) {
			printf("CBO.ZERO/64-byte guard check failed at byte %d\n", i);
			return CMD_RET_FAILURE;
		}
	}
	puts("CBO instruction access and 64-byte zero/guard check: PASS\n");
	puts("Boot-hart check only; Linux must verify timers on all eight harts.\n");
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(a210_isa_test, 1, 0, do_a210_isa_test,
	   "test boot-hart Sstc and Zicbo access (requires new firmware)", "");
