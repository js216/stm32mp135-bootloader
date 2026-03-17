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

### Stage 1 — Remove old recovery infrastructure

**Goal:** delete all NAND-recovery and DTB-patching code; confirm normal boot
still works.

**Files to change:**

| File | Action |
|------|--------|
| `src/dtb.c` | Delete |
| `src/dtb.h` | Delete |
| `src/fmc.c` | Remove `#ifdef PARSE_DTB` include block (lines 21–23); delete `fmc_bload_recovery` function (lines 928–1003) |
| `src/fmc.h` | Remove `fmc_bload_recovery` declaration (line 30) |
| `src/cmd.c` | Remove `fmc_bload_recovery` entry from `cmd_list[]` (lines 269–276) |
| `scripts/nandimage.py` | Remove `--recovery` argument and its body (lines 86–87, 136–143) |

No Makefile changes needed (`PARSE_DTB` was only guarded in source, not set
in the Makefile).

**Hardware test:**
```
> fmc_bload
> jump
# kernel boots normally from NAND
# 'help' no longer lists fmc_bload_recovery
```

---

### Stage 2 — initrd relocation

**Goal:** `fmc_bload` silently relocates an initrd present in the USB DDR buffer
to `DEF_INITRD_ADDR`; `relocate_initrd` command available for manual use.

**`src/defaults.h`** — add (if not already present):
```c
#define DEF_INITRD_ADDR 0xCA000000
#define DEF_INITRD_END  0xCC000000
```

Elsewhere (`stm32mp13xx-ddr-4Db.h`, or a different file for a different size
memory) declares the memory size:

```c
#define DDR_MEM_SIZE  0x20000000
```

`src/defaults.h` should do a compile-time check that everything will actually
fit into the chosen memory size.

**`src/fmc.c`** — at the very top of `fmc_bload()`, before reading the
partition table, insert:
```c
/* Relocate initrd from USB DDR buffer if present (gzip magic). */
{
   const uint8_t *h = (const uint8_t *)FMC_DDR_BUF_ADDR;
   if (h[0] == 0x1fu && h[1] == 0x8bu) {
      my_printf("bload: initrd at 0x%08lx -> 0x%08lx (%lu B)\r\n",
                (unsigned long)FMC_DDR_BUF_ADDR,
                (unsigned long)DEF_INITRD_ADDR,
                (unsigned long)(DEF_INITRD_END - DEF_INITRD_ADDR));
      memcpy((void *)DEF_INITRD_ADDR, (const void *)FMC_DDR_BUF_ADDR,
             DEF_INITRD_END - DEF_INITRD_ADDR);
   }
}
```

**`src/ddr.c`** — add:
```c
void ddr_relocate_initrd(int argc, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
   (void)argc; (void)arg1; (void)arg2; (void)arg3;
   my_printf("relocate_initrd: 0x%08lx -> 0x%08lx (%lu B)\r\n",
             (unsigned long)FMC_DDR_BUF_ADDR,
             (unsigned long)DEF_INITRD_ADDR,
             (unsigned long)(DEF_INITRD_END - DEF_INITRD_ADDR));
   memcpy((void *)DEF_INITRD_ADDR, (const void *)FMC_DDR_BUF_ADDR,
          DEF_INITRD_END - DEF_INITRD_ADDR);
}
```

**`src/ddr.h`** — add declaration for `ddr_relocate_initrd`.

**`src/cmd.c`** — add entry (no `NAND_FLASH` guard needed; DDR is always present):
```c
{
   .name    = "relocate_initrd",
   .syntax  = "",
   .summary = "Copy 32 MiB from USB DDR buffer to DEF_INITRD_ADDR",
   .handler = ddr_relocate_initrd,
},
```

**Hardware tests:**

Test A — normal production boot (no initrd in buffer):
```
> fmc_bload
# no "initrd" line printed
> jump
# kernel mounts rootfs from NAND
```

Test B — factory recovery boot:
```
# host: dd if=recovery.cpio.gz of=/dev/sdX bs=512
> fmc_bload
# prints "bload: initrd at 0xC8000000 -> 0xCA000000 ..."
> jump
# kernel boots recovery initrd, you get a shell
```

Test C — manual relocation (fallback):
```
# host: dd if=recovery.cpio.gz of=/dev/sdX bs=512
> relocate_initrd
> fmc_bload    # auto-detects again — harmless double copy
> jump
```

---

## Note on step 9 / production boot gzip-magic check

After `fmc_flush` in step 8, the DDR buffer holds `update.img`.  The first
bytes of `update.img` are the STM32 bootloader binary header — not `0x1f 0x8b`
— so the initrd check in `fmc_bload` silently passes with no relocation.
Production boot is unaffected.
