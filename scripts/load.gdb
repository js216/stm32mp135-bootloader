# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Stanford Research Systems, Inc.

file build/vmlinux
directory linux
target remote localhost:2330
monitor reset
monitor flash device=STM32MP135F
load build/main.elf
monitor go
b arch/arm/mm/cache-v7.S:40
