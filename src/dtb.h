// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file dtb.h
 * @brief In-memory DTB patching
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef DTB_H
#define DTB_H

#include <stdint.h>

/*
 * Patch the in-memory DTB at DEF_DTB_ADDR:
 *   - Set linux,initrd-start and linux,initrd-end in /chosen to the supplied
 *     addresses (both properties must already exist in the DTB).
 *   - Strip UBI tokens (ubi.mtd=rootfs, root=ubi0:rootfs, rootfstype=ubifs)
 *     from the bootargs property in /chosen so the kernel boots from initrd.
 * Returns 0 on success, -1 if any required property was not found.
 */
int dtb_patch_initrd(uint32_t initrd_start, uint32_t initrd_end);

#endif /* DTB_H */

// end file dtb.h
