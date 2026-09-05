# A210 DEV vendor inputs

Copied unchanged from openeuler-riscv/bootloader-build commit
9f2231ab48623560f413101fa8c192a2af2ce6d8, `zhihe-a210/blobs/`.
The upstream packaging workflow pairs these with vendor U-Boot
0d2b9acde3cb4162cb40dc5ccbcad75c36a56a03 and vendor OpenSBI
d9cfcff67e68f44680df42b379a69e403f1b0b5f (board release v2.9.0).

bootzero2.bin and bootzero-rvbl.bin are proprietary platform initialization
firmware. Their redistribution license is unspecified by the source
repository (its README says "TBD"). They are kept here as local build
inputs, not relicensed under U-Boot's GPL. Do not publish them without
checking the vendor's redistribution terms.

a210-opensbi.dtb is the original firmware device tree. prepare-fdt.sh
only modifies a build-directory copy. SHA256SUMS pins all three inputs.
