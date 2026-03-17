# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 Stanford Research Systems, Inc.

"""
Build a NAND flash image with a partition table.

Block layout (matches nand_pt.h):
  0    bootloader primary   (ROM scans 0..3 for STM32 header)
  1    bootloader redundant
  2    partition table
  3    DTB
  4+   kernel
  N+   rootfs
"""

import argparse
import struct
import sys
from pathlib import Path

# NAND geometry (must match board.h)
PAGE  = 4096
BLOCK = 64 * PAGE          # 256 KiB per block

# Partition table constants (must match nand_pt.h)
PT_MAGIC     = 0x4E414E44  # "NAND"
PT_VERSION   = 1
PT_MAX_PARTS = 8

BLOCK_BOOT       = 0
BLOCK_PT         = 2
BLOCK_DTB        = 3
BLOCK_KERNEL     = 4
KERNEL_MAX_BLOCKS = 64   # kernel partition is always this many blocks; rootfs follows after
BLOCK_ROOTFS     = BLOCK_KERNEL + KERNEL_MAX_BLOCKS  # 68

# struct nand_part_t:  char name[16], uint32 start_block, uint32 num_blocks
PART_FMT  = '<16sII'
# struct nand_pt_t header: uint32 magic, version, total_blocks, num_parts
PT_HDR_FMT = '<IIII'


def nblocks(n_bytes):
    return (n_bytes + BLOCK - 1) // BLOCK


def write_block(img, block_idx, data):
    """Write data at block_idx, 0xFF-padded to a whole number of blocks."""
    padded = data + b'\xff' * ((-len(data)) % BLOCK)
    img.seek(block_idx * BLOCK)
    img.write(padded)


def make_partition_table(total_blocks, parts):
    """
    parts: list of (name_str, start_block, num_blocks)
    Returns bytes for the full nand_pt_t struct.
    """
    header = struct.pack(PT_HDR_FMT, PT_MAGIC, PT_VERSION,
                         total_blocks, len(parts))
    parts_data = b''
    for name, start, n in parts:
        name_b = (name.encode('ascii') + b'\x00' * 16)[:16]
        parts_data += struct.pack(PART_FMT, name_b, start, n)
    # Pad to PT_MAX_PARTS entries
    empty = struct.pack(PART_FMT, b'\x00' * 16, 0, 0)
    parts_data += empty * (PT_MAX_PARTS - len(parts))

    body = header + parts_data
    checksum = sum(body) & 0xFFFFFFFF
    return body + struct.pack('<I', checksum)


def main():
    parser = argparse.ArgumentParser(
        description='Build a NAND flash image with partition table')
    parser.add_argument('image',   help='Output image file')
    parser.add_argument('--boot',   required=True, metavar='FILE',
                        help='Bootloader binary (.stm32)')
    parser.add_argument('--dtb',    metavar='FILE',
                        help='Device tree blob (.dtb)')
    parser.add_argument('--kernel', metavar='FILE',
                        help='Kernel image')
    parser.add_argument('--rootfs', metavar='FILE',
                        help='Root filesystem image')
    args = parser.parse_args()

    img_path = Path(args.image)
    img_path.write_bytes(b'')

    parts = []
    placements = []   # (label, block, raw_size_bytes)

    with img_path.open('r+b') as img:
        # Bootloader: primary at block 0, redundant at block 1
        boot_data = Path(args.boot).read_bytes()
        write_block(img, BLOCK_BOOT,     boot_data)
        write_block(img, BLOCK_BOOT + 1, boot_data)
        parts.append(('bootloader', BLOCK_BOOT, 2))
        placements.append((Path(args.boot).name + ' [0]', BLOCK_BOOT,     len(boot_data)))
        placements.append((Path(args.boot).name + ' [1]', BLOCK_BOOT + 1, len(boot_data)))

        # DTB at block 3 (fixed, even if absent — kernel still starts at 4)
        if args.dtb:
            dtb_data = Path(args.dtb).read_bytes()
            write_block(img, BLOCK_DTB, dtb_data)
            parts.append(('dtb', BLOCK_DTB, nblocks(len(dtb_data))))
            placements.append((Path(args.dtb).name, BLOCK_DTB, len(dtb_data)))

        # Kernel at block 4; always occupies KERNEL_MAX_BLOCKS regardless of actual size
        if args.kernel:
            kernel_data = Path(args.kernel).read_bytes()
            kernel_blks = nblocks(len(kernel_data))
            if kernel_blks > KERNEL_MAX_BLOCKS:
                print(f'ERROR: kernel is {kernel_blks} blocks, exceeds KERNEL_MAX_BLOCKS={KERNEL_MAX_BLOCKS}',
                      file=sys.stderr)
                sys.exit(1)
            write_block(img, BLOCK_KERNEL, kernel_data)
            parts.append(('kernel', BLOCK_KERNEL, kernel_blks))
            placements.append((Path(args.kernel).name, BLOCK_KERNEL, len(kernel_data)))

        # Rootfs always starts at BLOCK_ROOTFS (block 68) for a fixed MTD layout
        total_blocks = BLOCK_ROOTFS
        rootfs_blks  = 0
        if args.rootfs:
            rootfs_data = Path(args.rootfs).read_bytes()
            write_block(img, BLOCK_ROOTFS, rootfs_data)
            rootfs_blks = nblocks(len(rootfs_data))
            parts.append(('rootfs', BLOCK_ROOTFS, rootfs_blks))
            placements.append((Path(args.rootfs).name, BLOCK_ROOTFS, len(rootfs_data)))
            total_blocks = BLOCK_ROOTFS + rootfs_blks

        # Partition table at block 2 (written last, after total_blocks is known)
        parts.append(('ptable', BLOCK_PT, 1))
        pt_bytes = make_partition_table(total_blocks, parts)
        write_block(img, BLOCK_PT, pt_bytes)
        placements.append(('partition table', BLOCK_PT, len(pt_bytes)))

    # Summary
    placements.sort(key=lambda x: x[1])
    print()
    print('{:<28} {:>5}  {:>10}  {:>6}'.format('File', 'Block', 'Size', 'Blocks'))
    print('-' * 55)
    for label, blk, size in placements:
        print('{:<28} {:>5}  {:>10}  {:>6}'.format(
            label, blk, size, nblocks(size)))
    print()
    print(f'total_blocks = {total_blocks}')
    print(f'fmc_flush will use this automatically via partition table.')


if __name__ == '__main__':
    main()
