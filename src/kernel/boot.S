/*
 * EmberOS Boot Code
 * ARM64 (AArch64) startup for QEMU virt machine
 * 
 * Requirements: 1.1, 1.2
 * - Initialize ARM64 CPU in EL1
 * - Set up initial stack pointer
 * - Transfer control to kernel_main
 */

.section .text.boot
.global _start

_start:
    /*
     * Disable interrupts (IRQ and FIQ)
     * DAIF: Debug, SError, IRQ, FIQ mask bits
     */
    msr     daifset, #0xf

    /*
     * Check current exception level
     * We expect to be in EL1 when loaded by QEMU
     * CurrentEL is bits [3:2] of the register
     */
    mrs     x0, CurrentEL
    lsr     x0, x0, #2
    and     x0, x0, #3
    
    /* If we're in EL2, drop to EL1 */
    cmp     x0, #2
    b.eq    drop_to_el1
    
    /* If we're in EL1, continue to setup */
    cmp     x0, #1
    b.eq    el1_setup
    
    /* Unexpected EL, hang */
    b       hang

drop_to_el1:
    /*
     * Configure EL1 before dropping down
     * HCR_EL2: Hypervisor Configuration Register
     * Set RW bit (bit 31) for AArch64 execution in EL1
     */
    mov     x0, #(1 << 31)      // RW = 1 (AArch64)
    msr     hcr_el2, x0
    
    /*
     * Set up SPSR_EL2 for return to EL1h
     * M[3:0] = 0b0101 = EL1h (EL1 with SP_EL1)
     * DAIF = 0b1111 (all interrupts masked)
     */
    mov     x0, #0x3c5          // EL1h with interrupts masked
    msr     spsr_el2, x0
    
    /* Set ELR_EL2 to el1_setup address */
    adr     x0, el1_setup
    msr     elr_el2, x0
    
    /* Return to EL1 */
    eret

el1_setup:
    /*
     * Set up the stack pointer
     * Stack grows downward, so we use __stack_top
     */
    ldr     x0, =__stack_top
    mov     sp, x0

    /*
     * Zero out the BSS section
     * BSS contains uninitialized global/static variables
     */
    ldr     x0, =__bss_start
    ldr     x1, =__bss_end
    
zero_bss_loop:
    cmp     x0, x1
    b.ge    zero_bss_done
    str     xzr, [x0], #8       // Store zero and increment by 8 bytes
    b       zero_bss_loop

zero_bss_done:
    /*
     * Jump to kernel_main (C++ entry point)
     * This function should never return
     */
    bl      kernel_main

    /* If kernel_main returns, hang */
hang:
    wfe                         // Wait for event (low power)
    b       hang

.size _start, . - _start
