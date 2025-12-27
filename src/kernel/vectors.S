/*
 * EmberOS Exception Vector Table
 * ARM64 (AArch64) exception handling for QEMU virt machine
 * 
 * Requirements: 3.2, 3.3, 3.4
 * - Set up exception vector table at appropriate address
 * - Save CPU state and dispatch to appropriate handler
 * - Restore CPU state and return to interrupted code
 */

.section .text

/*
 * ARM64 Exception Vector Table
 * 
 * The vector table must be 2KB aligned (0x800 bytes)
 * Each entry is 128 bytes (0x80), allowing for 32 instructions
 * 
 * Layout:
 * Offset    Exception Type           Source
 * 0x000     Synchronous              Current EL with SP0
 * 0x080     IRQ                      Current EL with SP0
 * 0x100     FIQ                      Current EL with SP0
 * 0x180     SError                   Current EL with SP0
 * 0x200     Synchronous              Current EL with SPx
 * 0x280     IRQ                      Current EL with SPx
 * 0x300     FIQ                      Current EL with SPx
 * 0x380     SError                   Current EL with SPx
 * 0x400     Synchronous              Lower EL using AArch64
 * 0x480     IRQ                      Lower EL using AArch64
 * 0x500     FIQ                      Lower EL using AArch64
 * 0x580     SError                   Lower EL using AArch64
 * 0x600     Synchronous              Lower EL using AArch32
 * 0x680     IRQ                      Lower EL using AArch32
 * 0x700     FIQ                      Lower EL using AArch32
 * 0x780     SError                   Lower EL using AArch32
 */

.balign 0x800
.global exception_vector_table
exception_vector_table:

/* ============================================ */
/* Current EL with SP0 (not used in EmberOS)   */
/* ============================================ */

/* 0x000: Synchronous - Current EL with SP0 */
.balign 0x80
curr_el_sp0_sync:
    b       sync_handler

/* 0x080: IRQ - Current EL with SP0 */
.balign 0x80
curr_el_sp0_irq:
    b       irq_handler

/* 0x100: FIQ - Current EL with SP0 */
.balign 0x80
curr_el_sp0_fiq:
    b       fiq_handler

/* 0x180: SError - Current EL with SP0 */
.balign 0x80
curr_el_sp0_serror:
    b       serror_handler

/* ============================================ */
/* Current EL with SPx (main kernel handlers)  */
/* ============================================ */

/* 0x200: Synchronous - Current EL with SPx */
.balign 0x80
curr_el_spx_sync:
    b       sync_handler

/* 0x280: IRQ - Current EL with SPx */
.balign 0x80
curr_el_spx_irq:
    b       irq_handler

/* 0x300: FIQ - Current EL with SPx */
.balign 0x80
curr_el_spx_fiq:
    b       fiq_handler

/* 0x380: SError - Current EL with SPx */
.balign 0x80
curr_el_spx_serror:
    b       serror_handler

/* ============================================ */
/* Lower EL using AArch64 (user mode)          */
/* ============================================ */

/* 0x400: Synchronous - Lower EL AArch64 */
.balign 0x80
lower_el_aarch64_sync:
    b       sync_handler

/* 0x480: IRQ - Lower EL AArch64 */
.balign 0x80
lower_el_aarch64_irq:
    b       irq_handler

/* 0x500: FIQ - Lower EL AArch64 */
.balign 0x80
lower_el_aarch64_fiq:
    b       fiq_handler

/* 0x580: SError - Lower EL AArch64 */
.balign 0x80
lower_el_aarch64_serror:
    b       serror_handler

/* ============================================ */
/* Lower EL using AArch32 (not used)           */
/* ============================================ */

/* 0x600: Synchronous - Lower EL AArch32 */
.balign 0x80
lower_el_aarch32_sync:
    b       sync_handler

/* 0x680: IRQ - Lower EL AArch32 */
.balign 0x80
lower_el_aarch32_irq:
    b       irq_handler

/* 0x700: FIQ - Lower EL AArch32 */
.balign 0x80
lower_el_aarch32_fiq:
    b       fiq_handler

/* 0x780: SError - Lower EL AArch32 */
.balign 0x80
lower_el_aarch32_serror:
    b       serror_handler

/* ============================================ */
/* Exception Handlers                           */
/* ============================================ */

/*
 * Macro to save all general-purpose registers
 * Saves x0-x30, sp, elr_el1, spsr_el1
 * Stack frame size: 34 * 8 = 272 bytes (aligned to 16)
 */
.macro save_context
    /* Allocate stack frame (288 bytes for 36 registers, 16-byte aligned) */
    sub     sp, sp, #288
    
    /* Save x0-x29 */
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    
    /* Save x30 (link register) and the original sp */
    add     x0, sp, #288
    stp     x30, x0, [sp, #240]
    
    /* Save ELR_EL1 (exception return address) and SPSR_EL1 */
    mrs     x0, elr_el1
    mrs     x1, spsr_el1
    stp     x0, x1, [sp, #256]
    
    /* Save ESR_EL1 (exception syndrome) and FAR_EL1 (fault address) */
    mrs     x0, esr_el1
    mrs     x1, far_el1
    stp     x0, x1, [sp, #272]
.endm

/*
 * Macro to restore all general-purpose registers
 */
.macro restore_context
    /* Restore ELR_EL1 and SPSR_EL1 */
    ldp     x0, x1, [sp, #256]
    msr     elr_el1, x0
    msr     spsr_el1, x1
    
    /* Restore x0-x29 */
    ldp     x0, x1, [sp, #0]
    ldp     x2, x3, [sp, #16]
    ldp     x4, x5, [sp, #32]
    ldp     x6, x7, [sp, #48]
    ldp     x8, x9, [sp, #64]
    ldp     x10, x11, [sp, #80]
    ldp     x12, x13, [sp, #96]
    ldp     x14, x15, [sp, #112]
    ldp     x16, x17, [sp, #128]
    ldp     x18, x19, [sp, #144]
    ldp     x20, x21, [sp, #160]
    ldp     x22, x23, [sp, #176]
    ldp     x24, x25, [sp, #192]
    ldp     x26, x27, [sp, #208]
    ldp     x28, x29, [sp, #224]
    
    /* Restore x30 */
    ldr     x30, [sp, #240]
    
    /* Deallocate stack frame */
    add     sp, sp, #288
.endm

/*
 * Synchronous Exception Handler
 * Handles synchronous exceptions (syscalls, undefined instructions, etc.)
 * Requirements: 3.3
 */
sync_handler:
    save_context
    
    /* Pass stack pointer as argument (points to saved context) */
    mov     x0, sp
    
    /* Call C handler */
    bl      handle_sync_exception
    
    restore_context
    eret

/*
 * IRQ Handler
 * Handles hardware interrupts
 * Requirements: 3.3, 3.4
 */
irq_handler:
    save_context
    
    /* Pass stack pointer as argument */
    mov     x0, sp
    
    /* Call C handler */
    bl      handle_irq
    
    restore_context
    eret

/*
 * FIQ Handler
 * Handles fast interrupts (typically used for secure world)
 * Requirements: 3.3
 */
fiq_handler:
    save_context
    
    /* Pass stack pointer as argument */
    mov     x0, sp
    
    /* Call C handler */
    bl      handle_fiq
    
    restore_context
    eret

/*
 * SError Handler
 * Handles system errors (asynchronous aborts)
 * Requirements: 3.3
 */
serror_handler:
    save_context
    
    /* Pass stack pointer as argument */
    mov     x0, sp
    
    /* Call C handler */
    bl      handle_serror
    
    restore_context
    eret

/*
 * Install exception vector table
 * Sets VBAR_EL1 to point to our vector table
 */
.global install_exception_vectors
install_exception_vectors:
    ldr     x0, =exception_vector_table
    msr     vbar_el1, x0
    isb
    ret

.size exception_vector_table, . - exception_vector_table
