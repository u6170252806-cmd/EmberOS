/*
 * EmberOS Interrupt Handler Header
 * ARM64 exception handling and GIC driver
 * 
 * Requirements: 3.1, 3.2, 3.3, 3.4, 3.5
 */

#ifndef EMBEROS_INTERRUPTS_H
#define EMBEROS_INTERRUPTS_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace interrupts {

// Maximum number of IRQ handlers
constexpr uint32_t MAX_IRQ = 256;

// Common IRQ numbers for QEMU virt machine
constexpr uint32_t IRQ_TIMER = 30;      // Non-secure physical timer (PPI)
constexpr uint32_t IRQ_UART = 33;       // UART0 (SPI)

// Interrupt handler function type
using Handler = void (*)(uint32_t irq);

/*
 * Exception context structure
 * Matches the stack frame created by save_context macro in vectors.S
 */
struct ExceptionContext {
    uint64_t x[31];         // x0-x30
    uint64_t sp;            // Original stack pointer
    uint64_t elr;           // Exception Link Register (return address)
    uint64_t spsr;          // Saved Program Status Register
    uint64_t esr;           // Exception Syndrome Register
    uint64_t far;           // Fault Address Register
};

/*
 * Initialize interrupt handling
 * Sets up exception vectors and GIC
 * Requirements: 3.1, 3.2
 */
void init();

/*
 * Register handler for specific IRQ
 * Requirements: 3.3
 */
void register_handler(uint32_t irq, Handler handler);

/*
 * Unregister handler for specific IRQ
 */
void unregister_handler(uint32_t irq);

/*
 * Enable specific IRQ in GIC
 * Requirements: 3.1
 */
void enable_irq(uint32_t irq);

/*
 * Disable specific IRQ in GIC
 * Requirements: 3.1
 */
void disable_irq(uint32_t irq);

/*
 * Enable global interrupts (unmask IRQ in DAIF)
 */
void enable();

/*
 * Disable global interrupts (mask IRQ in DAIF)
 */
void disable();

/*
 * Check if interrupts are enabled
 */
bool is_enabled();

} // namespace interrupts

#endif // EMBEROS_INTERRUPTS_H
