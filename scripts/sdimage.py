# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 Stanford Research Systems, Inc.

import sys
from pathlib import Path

SECTOR = 512

# preferred LBAs for binaries
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
    """Use preferred LBA if it does not overlap previous data, otherwise next free LBA."""
    if preferred >= current:
        return preferred
    print(
        f"warning: {name} cannot be placed at preferred LBA {preferred}, "
        f"using {current} instead"
    )
    return current

def create_mbr(img, partitions):
    """Write a minimal MBR with the given partition entries.
    partitions: list of dicts with keys 'lba', 'sectors', 'type' (int), 'boot' (bool)
    """
    mbr = bytearray(512)
    # optional: bootloader code left zeroed

    for i, p in enumerate(partitions):
        if i >= 4:
            print(f"warning: only 4 partitions supported, skipping {p}")
            continue
        offset = 446 + i * 16
        boot_flag = 0x80 if p.get("boot", False) else 0x00
        part_type = p.get("type", 0x83)
        start_lba = p["lba"]
        num_sectors = p["sectors"]

        # CHS fields ignored
        mbr[offset + 0] = boot_flag
        mbr[offset + 1:offset + 4] = b"\x00\x00\x00"
        mbr[offset + 4] = part_type
        mbr[offset + 5:offset + 8] = b"\x00\x00\x00"
        mbr[offset + 8:offset + 12] = start_lba.to_bytes(4, "little")
        mbr[offset + 12:offset + 16] = num_sectors.to_bytes(4, "little")

    # MBR signature
    mbr[510] = 0x55
    mbr[511] = 0xAA

    img.seek(0)
    img.write(mbr)

def main():
    if len(sys.argv) < 3:
        print("Usage: sdimage.py image.img file1 [file2 ...] [--partition part1 ...]")
        sys.exit(1)

    img_path = Path(sys.argv[1])
    args = sys.argv[2:]

    # Separate files and partitions
    files = []
    partitions_args = []
    saw_partition = False

    i = 0
    while i < len(args):
        if args[i] == "--partition":
            saw_partition = True
            i += 1
            while i < len(args) and not args[i].startswith("--"):
                partitions_args.append(args[i])
                i += 1
        else:
            if saw_partition:
                print(f"warning: non-partition file '{args[i]}' comes after --partition; "
                      "this may overlap partition space")
            files.append(args[i])
            i += 1

    files = [Path(p) for p in files]
    partitions_args = [Path(p) for p in partitions_args]

    # create or truncate image
    with img_path.open("wb") as f:
        f.truncate(0)

    placements = []
    partition_infos = []

    with img_path.open("r+b") as img:
        current_lba = 0

        # --- write ordinary files ---
        for i, p in enumerate(files):
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

        # --- write partition files ---
        for p in partitions_args:
            data = p.read_bytes()
            lba = choose_lba(current_lba, current_lba, p.name)  # no preferred LBA yet
            size = write_aligned(img, lba, data)
            current_lba = lba + (size // SECTOR)
            partition_infos.append({
                "name": p.name,
                "lba": lba,
                "sectors": size // SECTOR,
                "type": 0x83,
                "boot": False
            })

        # --- write MBR for partitions ---
        if partition_infos:
            create_mbr(img, partition_infos)

    # --- summary ---
    print("\n{:<25} {:<8} {:<10} {:<8}".format("File", "LBA", "Size", "Blocks"))
    print("-" * 55)
    for name, lba, size in placements:
        print("{:<25} {:<8} {:<10} {:<8}".format(
            name, lba, size, size // SECTOR
        ))
    for p in partition_infos:
        print("{:<25} {:<8} {:<10} {:<8}".format(
            p["name"], p["lba"], p["sectors"]*SECTOR, p["sectors"]
        ))

if __name__ == "__main__":
    main()
