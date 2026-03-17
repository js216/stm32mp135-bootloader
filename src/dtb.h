// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file dtb.h
 * @brief In-memory DTB patching
 * @author Jakob Kastelic
 * @copyright 2026 Stanford Research Systems, Inc.
 */

#ifndef DTB_H
#define DTB_H

#ifdef PARSE_DTB

#include <stdint.h>

/*
 * Patch linux,initrd-start and linux,initrd-end in the /chosen node of the
 * in-memory DTB at DEF_DTB_ADDR.  Both properties must already exist in the
 * DTB (with any placeholder values); only their values are overwritten.
 * Returns 0 on success, -1 if either property was not found.
 */
int dtb_patch_initrd(uint32_t initrd_start, uint32_t initrd_end);

#endif // PARSE_DTB

#endif // DTB_H

// end file dtb.h
