// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file boot.S
 * @brief Handoff to application
 * @author Jakob Kastelic
 * @copyright 2025 Stanford Research Systems, Inc.
 */

#include "defaults.h"

#define CPSR_MODE_SVC	0x13U
#define CPSR_I		(1U << 7)
#define CPSR_F		(1U << 6)
#define SCR_NS		(1U << 0)
#define ACTLR_SMP	(1U << 6)
#define SCTLR_M		(1U << 0)
#define SCTLR_C		(1U << 2)
#define SCTLR_I		(1U << 12)
#define SCTLR_TE	(1U << 30)
#define SCTLR_SPAN	(1U << 23)
#define SCTLR_A		(1U << 1)

.syntax unified
.text

.align 5
sm_vect_table:
   b .            // Reset
   b .            // Undefined instruction
   b sm_smc_entry // Secure monitor call
   b .            // Prefetch abort
   b .            // Data abort
   b .            // Reserved
   b .            // IRQ
   b .            // FIQ


.align 5
ns_debug_vectors:
    b .  // Reset
    b .  // Undefined Instruction
    b .  // SVC
    b .  // Prefetch Abort
    b .  // Data Abort
    b .  // Reserved
    b .  // IRQ
    b .  // FIQ


.align 2
sm_smc_entry:
   // Update SCR
   mrc p15, 0, r0, c1, c1, 0 // read SCR
   orr r0, r0, #SCR_NS
   mcr p15, 0, r0, c1, c1, 0 // write SCR

   // Setup Non-Secure Vector Base
   ldr r0, =ns_debug_vectors
   mcr p15, 0, r0, c12, c0, 0 // VBAR_NS

   // Boot arguments
   mov r0, #0
   mov r1, #0
   ldr r2, =DEF_DTB_ADDR

   // Will switch to SVC mode with IRQ and FIQ disabled
   mov r4, #(CPSR_MODE_SVC | CPSR_I | CPSR_F)

   // Return from the secure monitor call
   push  {r4} // CPSR after return
   push  {r3} // PC after return
   rfefd sp


.global handoff_jump
.type handoff_jump, %function
.align 2
handoff_jump:
   // Store app entry address for later jump
   mov     r3, r0

   // Set monitor vector table
   ldr r0, =sm_vect_table
   mcr p15, 0, r0, c12, c0, 1 // MVBAR

   // Grant Non-Secure Access to VFP/NEON (CP10/CP11)
   mrc p15, 0, r0, c1, c1, 2 // read NSACR
   orr r0, r0, #(3 << 10)    // Set bits 10 and 11
   mcr p15, 0, r0, c1, c1, 2 // write NSACR

   // Call the Secure Monitor handler
   smc #0
