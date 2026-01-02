# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Stanford Research Systems, Inc.

import sys
from pathlib import Path

SECTOR = 512

# preferred LBAs
LBA1 = 128   # always used
LBA2 = 640   # used if possible
LBA3 = 896   # used if possible

def write_aligned(img, lba, data):
    """Write to image at the given LBA, padded to 512 bytes. Returns padded size."""
    pad = (-len(data)) % SECTOR
    data_padded = data + b"\x00" * pad
    img.seek(lba * SECTOR)
    img.write(data_padded)
    return len(data_padded)

def choose_lba(preferred, current, name):
    """Use preferred LBA if it does not overlap, otherwise warn and use current."""
    if preferred >= current:
        return preferred

    print(
        f"warning: {name} cannot be placed at preferred LBA {preferred}, "
        f"using {current} instead"
    )
    return current

def main():
    if len(sys.argv) < 3:
        print("Usage: sdimage.py image.img bin1 [bin2 ...]")
        sys.exit(1)

    img_path = Path(sys.argv[1])
    bin_paths = [Path(p) for p in sys.argv[2:]]

    # create or truncate image
    with img_path.open("wb") as f:
        f.truncate(0)

    placements = []

    with img_path.open("r+b") as img:
        current_lba = 0

        for i, p in enumerate(bin_paths):
            data = p.read_bytes()

            if i == 0:
                lba = LBA1
            elif i == 1:
                lba = choose_lba(LBA2, current_lba, p.name)
            elif i == 2:
                lba = choose_lba(LBA3, current_lba, p.name)
            else:
                lba = current_lba

            size = write_aligned(img, lba, data)
            placements.append((p.name, lba, size))
            current_lba = lba + (size // SECTOR)

    # --- summary ---
    print("\n{:<25} {:<8} {:<10} {:<8}".format("File", "LBA", "Size", "Blocks"))
    print("-" * 55)
    for name, lba, size in placements:
        print("{:<25} {:<8} {:<10} {:<8}".format(
            name, lba, size, size // SECTOR
        ))

if __name__ == "__main__":
    main()

