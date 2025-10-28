// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright Hangfan Li <lihangfan@iscas.ac.cn>
 *
 * ZHIHE A210 BROM requires SPL image to be shipped in specified header format.
 *
 * This module implements support in mkimage and dumpimage for this file format.
 *
 * The related tools are either released within vendor U-Boot or buildroot,
 * which is GPL-2.0.
 */

#include <compiler.h>
#include <fcntl.h>
#include <u-boot/crc.h>
#include <sys/stat.h>
#include <string.h>
#include "imagetool.h"

#define BOOTHDR_SIZE 2048
#define BOOTHDR_MAGIC 0x4C4256520010006F
/**
 * struct boot_hdr - header for boot image on A210
 *
 * All fields are low-endian.
 */
struct boot_hdr {
	/** @magic: MAGIC value, always 0x4C4256520010006F */
	uint64_t magic;
	/** @payload_offset: offset to payload*/
	uint32_t payload_offset;
	/** @payload_size: size of payload*/
	uint32_t payload_size;
	/** @zero:	set to zero */
	uint8_t zero[BOOTHDR_SIZE - 16];
};

static int zha210_check_params(struct image_tool_params *params)
{
	/* Only the RISC-V architecture is supported */
	if (params->Aflag && params->arch != IH_ARCH_RISCV)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int zha210_verify_header(unsigned char *buf, int size,
			       struct image_tool_params *params)
{
	struct boot_hdr *hdr = (void *)buf;
	uint64_t hdr_magic = le64_to_cpu(hdr->magic);
	uint32_t payload_offset = le32_to_cpu(hdr->payload_offset);
	uint32_t payload_size = le32_to_cpu(hdr->payload_size);

	if (size < 0 ||
	    (size_t)size < sizeof(struct boot_hdr)) {
		printf("Truncated file\n");
		return EXIT_FAILURE;
	}
	if (hdr_magic != BOOTHDR_MAGIC) {
		printf("Unexpected magic value\n");
		return EXIT_FAILURE;
	}
	if (payload_offset < BOOTHDR_SIZE) {
		printf("Invalid payload offset: 0x%x\n",
		       payload_offset);
		return EXIT_FAILURE;
	}
	if ((size_t)size != payload_offset + payload_size){
		printf("Unexpected file size, expected %u bytes, provided %u bytes\n",
		       payload_offset + payload_size, size);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void zha210_print_header(const void *buf,
			       struct image_tool_params *params)
{
	struct boot_hdr *hdr = (void *)buf;
	uint32_t payload_offset = le32_to_cpu(hdr->payload_offset);
	uint32_t payload_size = le32_to_cpu(hdr->payload_size);

	printf("SPL size (padded): %u\n", payload_offset - BOOTHDR_SIZE);
	printf("Payload size: %u\n", payload_size);
}

static int zha210_check_image_type(uint8_t type)
{
	if (type == IH_TYPE_ZHA210)
		return EXIT_SUCCESS;

	return EXIT_FAILURE;
}

static int append_file(const char *src, const char *dest) {
	int ret = open(src, O_RDONLY);
	if (ret < 0) {
		printf("ERROR: Failed to open src \"%s\": %s\n", src, strerror(errno));
		return ret;
	}
	int src_fd = ret;
	ret = open(dest, O_WRONLY | O_APPEND);
	if (ret < 0) {
		printf("ERROR: Failed to open dest \"%s\": %s\n", dest, strerror(errno));
		goto err_open_dest;
	}
	int dest_fd = ret;

#define COPY_BUFFER_SIZE 65536
	void *buf = malloc(COPY_BUFFER_SIZE);
	if (buf == NULL) {
		printf("malloc() failed\n");
		goto err_malloc_buf;
	}
	for (;;) {
		ret = read(src_fd, buf, COPY_BUFFER_SIZE);
		if (ret < 0) {
			printf("ERROR: Failed to read src: %s\n", strerror(errno));
			goto err_io_failure;
		} else if (ret == 0) {
			break;
		}
		ret = write(dest_fd, buf, ret);
		if (ret < 0) {
			printf("ERROR: Failed to write dest: %s\n", strerror(errno));
			goto err_io_failure;
		}
	}

err_io_failure:
	free(buf);
err_malloc_buf:
	close(dest_fd);
err_open_dest:
	close(src_fd);
	return ret;
}

static void zha210_set_header(void *buf, struct stat *sbuf, int infd,
			     struct image_tool_params *params)
{
	struct boot_hdr *hdr = buf;

	char *strtok_saveptr = NULL;
	char *curr_file = strtok_r(params->datafile, ":", &strtok_saveptr);

	if (curr_file == NULL) {
		printf("ERROR: Mandatory SPL image not provided\n");
		return;
	}
	struct stat filestat = {};
	int ret = stat(curr_file, &filestat);
	if (ret != 0) {
		printf("ERROR: Failed to stat \"%s\": %s\n", curr_file, strerror(errno));
		return;
	}
	size_t payload_offset = BOOTHDR_SIZE + filestat.st_size;
	payload_offset = (payload_offset + 15) & ~15;
	hdr->payload_offset = cpu_to_le32(payload_offset);
	hdr->payload_size = cpu_to_le32(0);

	if (append_file(curr_file, params->imagefile) != 0) {
		printf("ERROR: Failed to append file\n");
		return;
	}
	if (truncate(params->imagefile, payload_offset) != 0) {
		printf("ERROR: Failed to truncate file\n");
		return;
	}

	curr_file = strtok_r(NULL, ":", &strtok_saveptr);
	if (curr_file != NULL) {
		// Payload is provided.
		ret = stat(curr_file, &filestat);
		if (ret != 0) {
			printf("ERROR: Failed to stat \"%s\": %s\n", curr_file, strerror(errno));
			return;
		}
		hdr->payload_size = cpu_to_le32(filestat.st_size);

		if (append_file(curr_file, params->imagefile) != 0) {
			printf("ERROR: Failed to append file\n");
			return;
		}
	}

	hdr->magic = cpu_to_le64(BOOTHDR_MAGIC);
}

static struct boot_hdr zh_a210_hdr;

U_BOOT_IMAGE_TYPE(
	zha210, /* id */
	"ZHIHE A210 BootImage", /* name */
	sizeof(struct boot_hdr), /* header_size */
	&zh_a210_hdr, /* header */
	zha210_check_params, /* check_params */
	zha210_verify_header, /* verify header */
	zha210_print_header, /* print header */
	zha210_set_header, /* set header */
	NULL, /* extract_subimage */
	zha210_check_image_type, /* check_image_type */
	NULL, /* fflag_handle */
	NULL /* vrec_header */
);
