# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 Stanford Research Systems, Inc.

import sys
from pathlib import Path

# NAND geometry (must match board.h)
PAGE  = 4096
BLOCK = 64 * PAGE   # 64 pages per block = 256 KiB

# The STM32MP13 ROM bootloader scans blocks 0..3 for a valid STM\x32 header.
# Place the bootloader at block 0 unconditionally.
BOOT_BLOCK = 0

def pages(n_bytes):
    """Round n_bytes up to the nearest whole page count."""
    return (n_bytes + PAGE - 1) // PAGE

def blocks(n_bytes):
    """Round n_bytes up to the nearest whole block count."""
    return (n_bytes + BLOCK - 1) // BLOCK

def write_aligned(img, block_offset, data):
    """Write data to img at block_offset, padded to a whole number of blocks.
    Returns the padded size in bytes."""
    pad = (-len(data)) % BLOCK
    data_padded = data + b"\xff" * pad   # NAND erased state is 0xFF
    img.seek(block_offset * BLOCK)
    img.write(data_padded)
    return len(data_padded)

def main():
    if len(sys.argv) < 3:
        print("Usage: nandimage.py image.bin bootloader.stm32 "
              "[--partition file1 file2 ...]")
        sys.exit(1)

    img_path = Path(sys.argv[1])
    args = sys.argv[2:]

    # Parse args: first positional is the bootloader, then --partition files
    files = []
    partition_args = []
    i = 0
    while i < len(args):
        if args[i] == "--partition":
            i += 1
            while i < len(args) and not args[i].startswith("--"):
                partition_args.append(Path(args[i]))
                i += 1
        else:
            files.append(Path(args[i]))
            i += 1

    if not files:
        print("error: no bootloader file given")
        sys.exit(1)

    bootloader = files[0]
    if len(files) > 1:
        print(f"warning: extra positional files ignored: "
              f"{[str(f) for f in files[1:]]}")

    # Create/truncate image
    img_path.write_bytes(b"")

    placements = []   # (name, block_offset, size_bytes)

    with img_path.open("r+b") as img:
        # Bootloader at block 0 (primary) and block 1 (redundant).
        # The STM32MP13 ROM scans blocks 0..3 in order; if block 0 is bad
        # it will find the copy at block 1.
        data = bootloader.read_bytes()
        size = write_aligned(img, BOOT_BLOCK, data)
        placements.append((bootloader.name + " [primary]",   BOOT_BLOCK,     size))
        write_aligned(img, BOOT_BLOCK + 1, data)
        placements.append((bootloader.name + " [redundant]", BOOT_BLOCK + 1, size))
        current_block = BOOT_BLOCK + 2

        # Partition files placed contiguously after the bootloader
        for p in partition_args:
            data = p.read_bytes()
            size = write_aligned(img, current_block, data)
            placements.append((p.name, current_block, size))
            current_block += blocks(size)

    # Summary — block offsets are what the bootloader needs to find each image
    print("\n{:<25} {:<8} {:<10} {:<8}".format("File", "Block", "Size", "Blocks"))
    print("-" * 55)
    for name, blk, size in placements:
        print("{:<25} {:<8} {:<10} {:<8}".format(name, blk, size, size // BLOCK))

if __name__ == "__main__":
    main()
