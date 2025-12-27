/*
 * EmberOS Interrupt Handler Implementation
 * ARM64 exception handling
 * 
 * Requirements: 3.2, 3.3, 3.4
 */

#include "interrupts.h"
#include "uart.h"

// Forward declaration of GIC functions (implemented in gic.cpp)
namespace gic {
    void init();
    void enable_irq(uint32_t irq);
    void disable_irq(uint32_t irq);
    uint32_t acknowledge_irq();
    void end_irq(uint32_t irq);
}

// External assembly function to install vector table
extern "C" void install_exception_vectors();

namespace interrupts {

// IRQ handler table
static Handler irq_handlers[MAX_IRQ] = {nullptr};

// Interrupt enabled state
static bool interrupts_enabled = false;

/*
 * Initialize interrupt handling
 * Requirements: 3.1, 3.2
 */
void init() {
    // Install exception vector table
    // Requirements: 3.2
    install_exception_vectors();
    
    // Initialize GIC
    // Requirements: 3.1
    gic::init();
    
    uart::puts("[interrupts] Exception vectors installed\n");
    uart::puts("[interrupts] GIC initialized\n");
}

/*
 * Register handler for specific IRQ
 * Requirements: 3.3
 */
void register_handler(uint32_t irq, Handler handler) {
    if (irq < MAX_IRQ) {
        irq_handlers[irq] = handler;
    }
}

/*
 * Unregister handler for specific IRQ
 */
void unregister_handler(uint32_t irq) {
    if (irq < MAX_IRQ) {
        irq_handlers[irq] = nullptr;
    }
}

/*
 * Enable specific IRQ in GIC
 * Requirements: 3.1
 */
void enable_irq(uint32_t irq) {
    gic::enable_irq(irq);
}

/*
 * Disable specific IRQ in GIC
 * Requirements: 3.1
 */
void disable_irq(uint32_t irq) {
    gic::disable_irq(irq);
}

/*
 * Enable global interrupts
 */
void enable() {
    interrupts_enabled = true;
    // Clear IRQ mask bit in DAIF
    asm volatile("msr daifclr, #2" ::: "memory");
}

/*
 * Disable global interrupts
 */
void disable() {
    // Set IRQ mask bit in DAIF
    asm volatile("msr daifset, #2" ::: "memory");
    interrupts_enabled = false;
}

/*
 * Check if interrupts are enabled
 */
bool is_enabled() {
    return interrupts_enabled;
}

} // namespace interrupts

/*
 * C exception handlers called from vectors.S
 * These handle the actual exception processing
 * Requirements: 3.3, 3.4
 */
extern "C" {

/*
 * Handle synchronous exceptions
 * (syscalls, undefined instructions, data aborts, etc.)
 */
void handle_sync_exception(interrupts::ExceptionContext* ctx) {
    // Extract exception class from ESR
    uint32_t ec = (ctx->esr >> 26) & 0x3F;
    
    uart::printf("[exception] Synchronous exception at 0x%x\n", ctx->elr);
    uart::printf("[exception] ESR: 0x%x (EC=0x%x)\n", ctx->esr, ec);
    uart::printf("[exception] FAR: 0x%x\n", ctx->far);
    
    // Decode exception class
    switch (ec) {
        case 0x00:
            uart::puts("[exception] Unknown reason\n");
            break;
        case 0x01:
            uart::puts("[exception] Trapped WFI/WFE\n");
            break;
        case 0x15:
            uart::puts("[exception] SVC instruction (syscall)\n");
            break;
        case 0x20:
            uart::puts("[exception] Instruction abort from lower EL\n");
            break;
        case 0x21:
            uart::puts("[exception] Instruction abort from same EL\n");
            break;
        case 0x24:
            uart::puts("[exception] Data abort from lower EL\n");
            break;
        case 0x25:
            uart::puts("[exception] Data abort from same EL\n");
            break;
        default:
            uart::printf("[exception] Exception class: 0x%x\n", ec);
            break;
    }
    
    // For now, halt on unhandled synchronous exceptions
    uart::puts("[exception] System halted\n");
    while (true) {
        asm volatile("wfe");
    }
}

/*
 * Handle IRQ interrupts
 * Requirements: 3.3, 3.4
 */
void handle_irq(interrupts::ExceptionContext* ctx) {
    (void)ctx;  // Context available if needed
    
    // Acknowledge the interrupt and get IRQ number
    uint32_t irq = gic::acknowledge_irq();
    
    // Check for spurious interrupt (IRQ 1023)
    if (irq == 1023) {
        return;
    }
    
    // Call registered handler if present
    if (irq < interrupts::MAX_IRQ && interrupts::irq_handlers[irq] != nullptr) {
        interrupts::irq_handlers[irq](irq);
    } else {
        uart::printf("[irq] Unhandled IRQ %d\n", irq);
    }
    
    // Signal end of interrupt to GIC
    gic::end_irq(irq);
}

/*
 * Handle FIQ interrupts
 * (Fast interrupts, typically used for secure world)
 */
void handle_fiq(interrupts::ExceptionContext* ctx) {
    (void)ctx;
    uart::puts("[exception] FIQ received (not implemented)\n");
}

/*
 * Handle SError (System Error)
 * (Asynchronous aborts)
 */
void handle_serror(interrupts::ExceptionContext* ctx) {
    uart::printf("[exception] SError at 0x%x\n", ctx->elr);
    uart::printf("[exception] ESR: 0x%x\n", ctx->esr);
    
    // System errors are typically fatal
    uart::puts("[exception] System halted due to SError\n");
    while (true) {
        asm volatile("wfe");
    }
}

} // extern "C"
