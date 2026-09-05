# A210 DEV: vendor U-Boot + OpenSBI v1.9 test firmware

Experimental, single-die DEV only. Building does not flash anything.
Keep the original vendor firmware and a working USB recovery procedure.
Do not overwrite the working kernel DTB or bootloader while first testing.

Source layout below `/home/revy/code/kernel/linux/a210`:

- `opensbi-zhihe-a210`: unchanged vendor reference.
- `opensbi-a210-v1.9`: v1.9 plus the A210 platform port.
- `u-boot-a210-vendor`: vendor U-Boot based on
  `0d2b9acde3cb4162cb40dc5ccbcad75c36a56a03`, with build/test changes.
- `firmware-build`: generated objects, firmware DTB and images.

## Build

The board uses `a210_evb_oerv_defconfig` even though the selected board is
A210 DEV. The default FIT configuration is a210-dev. The other vendor FIT
configuration names are not evidence of D2D support in this port.

On this host, SWIG 4.5 is incompatible with the vendor pylibfdt wrapper.
Use an isolated environment with SWIG 4.2.1, not changes to system Python:

```sh
cd /home/revy/code/kernel/linux/a210
python3 -m venv --system-site-packages firmware-build/host-tools
firmware-build/host-tools/bin/pip install swig==4.2.1 'setuptools<81' pyelftools pycryptodomex
sh u-boot-a210-vendor/tools/a210/build.sh
```

The build verifies the pinned vendor bootstrap blobs and builds the shared
DT from version-controlled sources. Standard DT ISA properties enable Sstc and
Zicbo in OpenSBI; U-Boot cannot itself write MENVCFG from S-mode.
`CONFIG_OF_BOARD_FIXUP` imports the actual OpenSBI reserved regions before
the common RISC-V fixup passes them to the Linux DT, avoiding assumptions
about the old firmware's memory footprint.

`verify.sh` runs at the end of the build. It checks all eight firmware CPU
descriptions and extracts the FIT payloads to compare them with the actual
build outputs, then verifies the FIT location in the eMMC image. These are
static packaging checks, not a substitute for an on-board boot test.

The development configuration also enables `A210_FIRMWARE_VERIFY`: RAM
FIT source hashes are checked explicitly without enabling the vendor's
required-key signature policy, loaded plain payloads are checked after
cache publication, and OpenSBI is checked again before the BRAM handoff.
Post-load verification errors now propagate to the loader. This profile
expects uncompressed, unencrypted SHA256 FIT payloads from this script.
It detects accidental corruption; unsigned FIT hashes are not secure boot.
The vendor PKSE signature selection is unchanged. A config signature
failure mentioning `key-dev` must not be interpreted as a SHA256 mismatch.

The build writes the immutable OpenSBI text fingerprint into the firmware
DT's `/chosen/zhihe,firmware-text-fnv1a` property. The debug firmware checks
against that value before accepting its baseline. Always test the complete
matched FIT; swapping only fw_dynamic.bin without regenerating its firmware
DT will intentionally fail. A fingerprint mismatch is not automatically
repaired or retried.

CONFIG_A210_SHARED_FDT packages one complete FIT DT, fdt-platform. SPL
loads it at 0x81000000, updates detected DRAM capacity and board identity,
and passes it to OpenSBI. U-Boot proper receives the same DT through a1
and uses OF_BOARD/OF_HAS_PRIOR_STAGE; its binary has no appended DTB.
Normal U-Boot relocation may copy the DT to another address, but it is
still the same logical description, not a separate board DT.

SPL retains an early control DT derived from the same source because it
must initialize hardware before loading the FIT. Linux still loads its
own DT through extlinux; it is outside this two-stage sharing change.
The shared DEV DT includes actual harts 0--7, C908/C920 ISA/cache data,
C900 PLIC, T-Head CLINT, UART4, PMU mappings and vendor device resources.
The separate a210-opensbi.dts has been removed. SPL SMP remains disabled.

The firmware PLIC now uses "zhihe,a210-plic", "thead,c900-plic" and two
interrupt cells. The shared source already describes interrupt formats;
verify-shared-dt.py checks the common fields in the build DT and shared runtime DT; and
prepare-plic.py is used only as a validator by the active build. OpenSBI's generic
C900 driver owns S-mode delegation and its resume handling. This requires
the matching OpenSBI platform change removing the private delegation code.

The firmware source contains no reset-sample node. The pinned legacy vendor
DTB is retained only for provenance, not consumed by this build. The
packaged input is named a210-firmware.dtb to prevent binman from selecting
an unprocessed build DT instead of the staged shared copy. The /binman
node is retained because CONFIG_BINMAN_FDT consumes it at runtime.
verify.sh also checks that
the FIT has exactly one flat_dt image and the U-Boot payload is nodtb.

Upstream U-Boot main at c46bd5b21361 describes C900 PLIC in DT but has no
generic C900 PLIC interrupt-dispatch driver to cherry-pick. Vendor A210
likewise had only declarations in light-plic.h and an empty weak
external_interrupt function; the existing binary contains only a return
instruction there. This DT change does not remove working IRQ dispatch.
Existing polled device operations remain; adding actual U-Boot PLIC IRQ
handling would require a separate architecture/IRQ-framework change.

Outputs in `firmware-build/dist/`:

- `bootzero-rvbl.bin`: unchanged vendor USB/RAM bootstrap.
- `binman-spl-with-fit-rvbl.bin`: RAM-test SPL + FIT.
- `binman-emmc_boot-loader.img`: persistent bootloader image; do not flash
  before RAM testing succeeds.

## RAM test first

Use the board's vendor USB recovery procedure (K1/K2) and the original
packaging project's RAM-only sequence. These commands are for the host,
after the board is in the expected fastboot recovery mode:

```sh
cd /home/revy/code/kernel/linux/a210/firmware-build/dist
fastboot flash ram bootzero-rvbl.bin
fastboot reboot
# Wait for fastboot to re-enumerate successfully.
fastboot flash ram binman-spl-with-fit-rvbl.bin
fastboot reboot
```

No eMMC write command is included here. Do not erase or save the U-Boot
environment as part of this test. The vendor recovery reference is:
https://github.com/openeuler-riscv/bootloader-build/tree/9f2231ab48623560f413101fa8c192a2af2ce6d8/zhihe-a210

OpenSBI should identify itself as v1.9 and print for each started hart:

```text
A210 hart0: sstc=1 zicbom=1 zicboz=1 menvcfg=...
A210 hart0: mcounteren=...
```

STCE (bit 63), CBZE (bit 7), CBCFE (bit 6), CBIE (bits 5:4 = 3), and
MCOUNTEREN.TM (bit 1) must be enabled. Remaining harts report when started
by Linux. The port checks these permissions but only hardware execution
can establish that the timer and cache operations actually work.

Stop at the U-Boot prompt and optionally run:

```text
a210_isa_test
```

This checks STIMECMP read/write on the boot hart without changing its
compare value, then executes CBO instructions on private aligned storage
and verifies CBO.ZERO clears 64 bytes without touching a guard line.
It does not prove timer interrupt delivery, cache coherency, prefetch
effectiveness, or support on the other seven harts. Running it with the
old firmware may trap; it has no exception-recovery mechanism.

## Linux test DTB

Leave the Linux source and working DTB untouched. Generate a separate DTB:

```sh
cd /home/revy/code/kernel/linux/a210
sh u-boot-a210-vendor/tools/a210/prepare-linux-dtb.sh \
  a210-linux/arch/riscv/boot/dts/zhihe/a210-dev.dtb \
  firmware-build/dist/a210-dev-sstc-zicbo.dtb
```

Use it only with the new firmware, via a separate boot entry or a one-time
boot. It preserves other nodes and adds Sstc/Zicbo plus 64-byte block sizes.
The kernel's existing MMIO timer workaround can remain: standard Sstc has
priority. Do not treat its early MMIO mapping message as proof of use.

Acceptance checks: Linux reports OpenSBI 1.9 and the Sstc timer path; all
eight CPUs are online; local timer counters progress on each CPU; eMMC
and rootfs initialize; no illegal CBO instructions or DMA corruption;
network works. Verify cold boot, reboot and CPU hotplug separately.
The old vendor 6.6 kernel uses MMIO S-mode compare and is not a valid
fallback under STCE=1 without adapting that timer path. Restore the
original firmware to return to the old vendor timer environment.

Blob licensing and provenance are recorded in `blobs/README.md`. No
OP-TEE dispatcher, private IOPMP SBI API or D2D support is included.
