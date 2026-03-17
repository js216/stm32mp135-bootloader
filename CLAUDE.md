# Factory flashing plan

## Overview

Production devices have no SD card; NAND is the only persistent storage.  The
"recovery" initrd (a small Linux with rootfs-writing tools) is never stored on
NAND.  It lives in DDR RAM only: loaded from the host over USB MSC, relocated
by the bootloader, and discarded on the next power cycle.

---

## DDR memory map at factory-boot time

```
0xC2000000   kernel          (fmc_bload loads from NAND partition "kernel")
0xC4000000   DTB             (fmc_bload loads from NAND partition "dtb")
0xC6000000   MMU tables
0xC8000000   FMC DDR buffer  ← USB MSC backing store (256 MiB)
             step 3: dd recovery.cpio.gz lands here (byte 0 of USB MSC)
0xCA000000   DEF_INITRD_ADDR ← relocation destination (32 MiB into buffer)
0xCC000000   DEF_INITRD_END  ← end of reserved initrd region
0xD8000000   end of FMC DDR buffer
```

No region overlaps at any step.  The relocation copies at most 32 MiB from
0xC8000000 to 0xCA000000; the source ends exactly where the destination starts,
so a plain `memcpy` is safe.

---

## defaults.h additions

```c
/* These two must match the /chosen node in the factory DTS. */
#define DEF_INITRD_ADDR 0xCA000000   /* start of initrd in DDR */
#define DEF_INITRD_END  0xCC000000   /* DEF_INITRD_ADDR + 32 MiB (fixed) */
```

---

## Recovery initrd format and build (Buildroot)

| Setting | Value |
|---------|-------|
| Buildroot option | `BR2_TARGET_ROOTFS_CPIO=y` + `BR2_TARGET_ROOTFS_CPIO_GZIP=y` |
| Pad to fixed size | `truncate --size=33554432 rootfs.cpio.gz` (32 × 1024 × 1024) |
| Detection in bootloader | gzip magic `0x1f 0x8b` at byte 0 |

`truncate` appends null bytes after the gzip end-of-stream marker.  The kernel
decompressor stops at the marker and ignores the trailing zeros.  The fixed size
means `DEF_INITRD_END` is a compile-time constant — no runtime DTB patching
needed, so `dtb.c` / `dtb.h` can be deleted entirely.

---

## DTS / DTB variants

Two compiled DTBs are needed; both come from the same base DTS:

**factory.dtb** — used for steps 1–7 (boot with initrd):
```dts
/ {
    chosen {
        linux,initrd-start = <0xCA000000>;   /* DEF_INITRD_ADDR */
        linux,initrd-end   = <0xCC000000>;   /* DEF_INITRD_END  */
    };
};
```

**production.dtb** — used for steps 8–10 (normal rootfs boot):
Omit the `linux,initrd-start` / `linux,initrd-end` lines entirely.

---

## Factory floor command sequence

```
1.  dd if=nand.img of=/dev/sdX bs=512
    # nand.img built with:
    #   nandimage.py --boot main.stm32 --dtb factory.dtb --kernel zImage
    # Contains: bootloader (×2), PT, factory DTB, kernel.  No rootfs.
    # total_blocks = BLOCK_ROOTFS = 68  →  fmc_flush will not touch block 68+.

2.  fmc_flush

3.  dd if=recovery.cpio.gz of=/dev/sdX bs=512
    # recovery.cpio.gz is exactly 32 MiB (truncate --size=33554432).

4.  fmc_bload
    # Detects gzip magic at FMC_DDR_BUF_ADDR, copies 32 MiB to DEF_INITRD_ADDR.
    # Then loads kernel and DTB (factory.dtb) from NAND as usual.

5.  jump

6.  Recovery Linux boots, mounts initrd at 0xCA000000.

7.  Recovery Linux comes up with SSH (real Ethernet).
    Technician SSHes in, downloads rootfs.ubi from the build server, writes
    it to NAND with ubiformat / nandwrite starting at block 68.

8.  Power cycle.  Press any key to stay in bootloader.
    dd if=update.img of=/dev/sdX bs=512
    # update.img built with:
    #   nandimage.py --boot main.stm32 --dtb production.dtb --kernel zImage
    # total_blocks = 68  →  fmc_flush writes only blocks 0-67 (rootfs untouched).
    fmc_flush

9.  fmc_bload
    # production.dtb has no initrd-start/end  →  gzip magic check fails
    # (buffer still has old data, but first bytes are NOT 0x1f 0x8b)  →
    # no relocation, normal boot.
    jump
    # kernel mounts rootfs from NAND.

10. Verify.
    Reflash bootloader built with BOOT_NOPROMPT for production.
```

---

## Implementation plan

### Stage 3 cleanup

- let's include the ddr constants in the board.h file
