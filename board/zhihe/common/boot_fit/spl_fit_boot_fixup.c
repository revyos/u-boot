// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <log.h>
#include <cpu_func.h>
#include <hang.h>
#include <errno.h>
#include <image.h>
#include <memalign.h>
#include <mapmem.h>
#include <spl.h>
#include <sysinfo.h>
#include <opensbi.h>

#include "../include/board_boot.h"
#include "../include/board_check.h"
#include "../include/board_porting.h"

/******************************
 * Fixup Kernel boot
 * The fit boot information in SPL is passed to U-Boot via FDT.
 ******************************/
static int fit_image_check(const void *fit, const char *image_name, int image_noffset, void *os_fdt)
{
    uint8_t type, arch, os;
    size_t size;
    ulong load, entry;
    const void *data;
    int ret;

    /* get os type */
    ret = fit_image_get_os(fit, image_noffset, &os);
    if (ret) {
        return -1;
    }

    /* check uboot os */
    if (os != IH_OS_U_BOOT) {
        return -1;
    }

    /*
     * get image info 
     * the image has been successfully loaded, and no further checks are required here
     */
    fit_image_get_load(fit, image_noffset, &load);
    fit_image_get_entry(fit, image_noffset, &entry);
    fit_image_get_data_and_size(fit, image_noffset, &data, &size);
    fit_image_get_type(fit, image_noffset, &type);
    fit_image_get_arch(fit, image_noffset, &arch);

    /* alloc fdt write space */
    ret = fdt_shrink_to_minimum(os_fdt, 8192);
    if (ret < 0) {
        return -1;
    }

    /* write image info to system fdt */
    fdt_record_loadable(os_fdt, 0 /*invalid*/, image_name, load, size, entry,
                        genimg_get_type_name(type), genimg_get_os_name(os),
                        genimg_get_arch_name(arch));

    return 0;
}

/* 
 * Parse fit fdt get uboot image info and write to OS fdt
 */
static int fit_os_fdt_fixup(void *fit_header, void *os_fdt)
{
    const char *fit_uname_config = NULL;
    const char *uname;
    int idx, conf_noffset, noffset;
    int ret;

    /* get default configurations node */
    conf_noffset = fit_conf_get_node((const void *)fit_header, fit_uname_config);
    if (conf_noffset < 0)
        return 0;

    /* loop through the images in the loadables list */
    for (idx = 0; uname = fdt_stringlist_get((const void *)fit_header, conf_noffset,
                                             FIT_LOADABLE_PROP, idx, NULL),
        uname;
         idx++) {
        noffset = fit_image_get_node(fit_header, uname);

        /* check image */
        ret = fit_image_check(fit_header, uname, noffset, os_fdt);

        if (ret == 0)
            break;
    }

    return 0;
}

/* Override weak imp at common/spl/spl_fit.c */
const char * board_get_fit_config(void)
{
	return spl_get_fit_dtb_name(1);
}

/******************************
 * Main fixups
 ******************************/
typedef void (*opensbi_entry_t)(ulong hartid, ulong dtb, ulong info);
 __weak void board_spl_call_opensbi(uintptr_t entry, ulong hartid, ulong dtb, ulong info)
{
    opensbi_entry_t opensbi_entry = (opensbi_entry_t)entry;
    opensbi_entry(hartid, dtb, info);
}

static uintptr_t s_opensbi_entry;

#ifdef CONFIG_A210_FIRMWARE_VERIFY
static const void *a210_sbi_fit;
static int a210_sbi_node;
static ulong a210_sbi_load, a210_sbi_size;

static int a210_verify_loaded(const void *fit, int node, ulong addr, ulong len)
{
	ALLOC_CACHE_ALIGN_BUFFER(uint8_t, digest, FIT_MAX_HASH_LEN);
	uint8_t *expected;
	int hash_node, digest_len, expected_len;
	const char *algo;
	const void *source;
	size_t source_len;
	uint8_t comp;

	if (fit_image_get_comp(fit, node, &comp) || comp != IH_COMP_NONE ||
	    fdt_subnode_offset(fit, node, "cipher") >= 0)
		return -ENOTSUPP;
	hash_node = fdt_subnode_offset(fit, node, "hash");
	if (hash_node < 0 || fit_image_hash_get_algo(fit, hash_node, &algo) ||
	    strcmp(algo, "sha256") ||
	    fit_image_hash_get_value(fit, hash_node, &expected, &expected_len))
		return -EBADMSG;
	if (expected_len != 32 || !len || addr + len < addr ||
	    addr + len > ULONG_MAX - (CONFIG_SYS_CACHELINE_SIZE - 1))
		return -EINVAL;

	/* Check plain source bytes without invoking FIT signature policy. */
	if (fit_image_get_data_and_size(fit, node, &source, &source_len) ||
	    source_len != len)
		return -EBADMSG;
	if (calculate_hash(source, source_len, algo, digest, &digest_len))
		return -EIO;
	if (digest_len != expected_len || memcmp(digest, expected, digest_len)) {
		printf("A210: source %s SHA256 MISMATCH, refusing to boot\n",
		       fdt_get_name(fit, node, NULL));
		return -EBADMSG;
	}
	printf("A210: source %s SHA256 OK\n", fdt_get_name(fit, node, NULL));

	/* Verify the destination after publishing the copied cache lines. */
	flush_dcache_range(addr & ~(CONFIG_SYS_CACHELINE_SIZE - 1UL),
			  ALIGN(addr + len, CONFIG_SYS_CACHELINE_SIZE));
	if (calculate_hash((const void *)addr, len, algo, digest, &digest_len))
		return -EIO;
	if (digest_len != expected_len || memcmp(digest, expected, digest_len)) {
		printf("A210: loaded %s SHA256 MISMATCH at 0x%lx, refusing to boot\n",
		       fdt_get_name(fit, node, NULL), addr);
		return -EBADMSG;
	}
	printf("A210: loaded %s SHA256 OK at 0x%lx (%lu bytes)\n",
	       fdt_get_name(fit, node, NULL), addr, len);
	return 0;
}
#endif

static void fixup_opensbi_entry(ulong hartid, ulong dtb, ulong info)
{
    struct fw_dynamic_info *opensbi_info = (struct fw_dynamic_info *)info;

#ifdef CONFIG_A210_FIRMWARE_VERIFY
	if (!a210_sbi_fit || a210_verify_loaded(a210_sbi_fit, a210_sbi_node,
					      a210_sbi_load, a210_sbi_size))
		hang();
#endif

	if (env_get_ulong("boot_loglevel", 10, 0) < 1)
        opensbi_info->options = 1; // disable opensbi log

	board_spl_call_opensbi(s_opensbi_entry, hartid, dtb, info);
}

/*
 * Fix the issue where the full fit mode cannot access the next level of OS entry
 * spl_perform_fixups is weak imp at u-boot/common/spl/spl.c
 */
void spl_perform_fixups(struct spl_image_info *spl_image)
{
    u64 start;
    u64 size;

#if defined(CONFIG_SPL_ENV_SUPPORT) && defined(CONFIG_SPL_FS_EXT4)
    /* reload dtb file */
    if (spl_boot_device() != BOOT_DEVICE_BOOTROM) {
        spl_load_dtb_from_bootfs();
    }
#endif

    /*
     * OS fdt fixup 
     */
    /* 1. Add u-boot info to kernel fdt for opensbi can boot to u-boot */
    fit_os_fdt_fixup(map_sysmem(CONFIG_SYS_LOAD_ADDR, 0), spl_image->fdt_addr);

    /* 2. Board user-define fdt fixup */
    if (spl_fixup_os_fdt(spl_image->fdt_addr) !=0 ) {
        printf("spl: Warning, failed fixup os fdt\n");
		if (IS_ENABLED(CONFIG_A210_FIRMWARE_VERIFY))
			hang();
    }

    /*
     * U-Boot fdt fixup 
     */
    /* 1. Fixup DDR size, write to u-boot fdt */
    void *fdt_uboot = IS_ENABLED(CONFIG_A210_SHARED_FDT) ?
        spl_image->fdt_addr : spl_find_uboot_fdt_blob();
    if (!fdt_uboot) {
        return;
    }
    debug("uboot fdt blob 0x%p\n", fdt_uboot);
    if (!IS_ENABLED(CONFIG_A210_SHARED_FDT) && spl_get_ddr_info(&start, &size) == 0) {
        int ret = fdt_fixup_memory(fdt_uboot, start, size);
        debug("fixup mem ret %d\n", ret);
        if (ret) {
            printf("spl: Warning, failed fixup memeory\n");
        }
    }

    /* 2. Set board type pass to u-boot */
    spl_set_binfo_to_uboot_fdt(fdt_uboot);

    /*
     * OpenSBI jump fixup
     */
    s_opensbi_entry = spl_image->entry_point;
    spl_image->entry_point = (uintptr_t)fixup_opensbi_entry;
}

#ifdef CONFIG_SPL_FIT_SIGNATURE
/* weak imp at common/spl/spl_fit.c */
int board_spl_fit_is_verify(void)
{
    struct zhihe_image_header *header = (struct zhihe_image_header *)(CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE);
    debug("Verify check: 0x%x 0x%x\n", header->magic, header->verify_next);
    if (header->magic == RVBL_MAGIC) {
        /* RVBL Header */
        return 0;
    } else if (header->magic == PKSE_MAGIC) {
        return header->verify_next ? 1 : 0;
    }
    return 1;
}
#endif

int board_fit_each_image_post_load(const void *fit, int noffset, ulong loadaddr, ulong len)
{
#ifdef CONFIG_A210_FIRMWARE_VERIFY
	uint8_t os;
	int ret = a210_verify_loaded(fit, noffset, loadaddr, len);

	if (ret)
		return ret;
	if (!fit_image_get_os(fit, noffset, &os) && os == IH_OS_OPENSBI) {
		a210_sbi_fit = fit;
		a210_sbi_node = noffset;
		a210_sbi_load = loadaddr;
		a210_sbi_size = len;
	}
	return 0;
#else
    ALLOC_CACHE_ALIGN_BUFFER(uint8_t, hash_value, FIT_MAX_HASH_LEN);
    int hash_value_len;
    const char *algo;

    uint8_t *fit_hash_value;
    int fit_hash_value_len;
    int noffset_hash;

    if (loadaddr == 0) {
        return -1;
    }

	if (env_get_ulong("boot_loglevel", 10, 0) < 3)
		return 0;

    //printf("fit %p, noffset %d\n", fit, noffset);
    noffset_hash = fdt_subnode_offset(fit, noffset, "hash");
    if (noffset_hash < 0) {
        //printf("spl: Can't get hash property\n");
        return -1;
    }

    if (fit_image_hash_get_algo(fit, noffset_hash, &algo)) {
        printf("spl: Can't get hash algo property\n");
        return -1;
    }

    //printf("fit %p, noffset_hash %d\n", fit, noffset_hash);
    if (fit_image_hash_get_value(fit, noffset_hash, &fit_hash_value, &fit_hash_value_len)) {
        printf("spl: Can't get hash value property\n");
        return -1;
    }

    if (calculate_hash((void *)loadaddr, len, algo, hash_value, &hash_value_len)) {
        printf("spl: Unsupported hash algorithm\n");
        return -1;
    }

    printf("     Uncompress size: %ld\n", len);
    printf("     Hash:            ");
    for (int i = 0; i < hash_value_len; i++) {
        printf("%02x", hash_value[i]);
    }
    printf("\n");

    // if (hash_value_len != fit_hash_value_len) {
    //     printf("spl: Bad hash value len\n");
    //     return -1;
    // } else if (memcmp(hash_value, fit_hash_value, hash_value_len) != 0) {
    //     printf("spl: Bad hash value\n");
    //     return -1;
    // }
    return 0;
#endif
}
