# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Stanford Research Systems, Inc.

import sys
from pathlib import Path

SECTOR = 512
FSBL1_LBA = 128
FSBL2_LBA = 640

def write_aligned(img, lba, data):
    """Write to image at the given LBA, padded to 512 bytes. Returns padded size."""
    pad = (-len(data)) % SECTOR
    data_padded = data + b"\x00" * pad
    offset = lba * SECTOR
    img.seek(offset)
    img.write(data_padded)
    return len(data_padded)

def main():
    if len(sys.argv) < 3:
        print("Usage: sdimage.py image.img fsbl.bin [other.bin ...] [--second-fsbl]")
        sys.exit(1)

    # detect the flag
    second_fsbl = False
    args = sys.argv[1:]
    if "--second-fsbl" in args:
        second_fsbl = True
        args.remove("--second-fsbl")

    img_path = Path(args[0])
    fsbl_path = Path(args[1])
    other_paths = [Path(p) for p in args[2:]]

    fsbl_data = fsbl_path.read_bytes()

    # Create or truncate image
    with img_path.open("wb") as f:
        f.truncate(0)

    placements = []

    with img_path.open("r+b") as img:
        # Write FSBL #1 (always)
        size1 = write_aligned(img, FSBL1_LBA, fsbl_data)
        placements.append((fsbl_path.name, FSBL1_LBA, size1))

        # Write FSBL #2 (only if flag)
        current_lba = FSBL1_LBA + (size1 // SECTOR)
        if second_fsbl:
            size2 = write_aligned(img, FSBL2_LBA, fsbl_data)
            placements.append((fsbl_path.name + " (copy)", FSBL2_LBA, size2))
            current_lba = FSBL2_LBA + (size2 // SECTOR)

        # Write extra binaries
        for p in other_paths:
            data = p.read_bytes()
            size = write_aligned(img, current_lba, data)
            placements.append((p.name, current_lba, size))
            current_lba += size // SECTOR

    # --- summary ---
    print("\n{:<25} {:<8} {:<10} {:<8}".format("File", "LBA", "Size", "Blocks"))
    print("-" * 55)
    for name, lba, size in placements:
        print("{:<25} {:<8} {:<10} {:<8}".format(name, lba, size, size // SECTOR + 1))

if __name__ == "__main__":
    main()
