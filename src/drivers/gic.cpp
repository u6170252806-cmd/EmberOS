/*
 * EmberOS GIC (Generic Interrupt Controller) Driver
 * ARM GICv2 implementation for QEMU virt machine
 * 
 * Requirements: 3.1
 * - Configure the ARM GIC (Generic Interrupt Controller)
 */

#include "gic.h"
#include "uart.h"

namespace gic {

// Memory-mapped I/O helpers
static inline void write32(uintptr_t addr, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(addr) = value;
}

static inline uint32_t read32(uintptr_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}

// Number of supported IRQs (detected during init)
static uint32_t num_irqs = 0;

/*
 * Initialize GIC Distributor (GICD)
 * The distributor routes interrupts to CPU interfaces
 */
static void init_distributor() {
    // Disable distributor while configuring
    write32(GICD_BASE + GICD_CTLR, 0);
    
    // Read number of supported IRQs from TYPER register
    // ITLinesNumber field (bits 4:0) indicates (N+1)*32 interrupt lines
    uint32_t typer = read32(GICD_BASE + GICD_TYPER);
    num_irqs = ((typer & 0x1F) + 1) * 32;
    
    uart::printf("[gic] Distributor supports %d IRQs\n", num_irqs);
    
    // Configure all SPIs (Shared Peripheral Interrupts, IRQ 32+)
    // IRQs 0-31 are SGIs (0-15) and PPIs (16-31), which are per-CPU
    for (uint32_t i = 32; i < num_irqs; i += 32) {
        // Disable all interrupts initially
        write32(GICD_BASE + GICD_ICENABLER + (i / 32) * 4, 0xFFFFFFFF);
        
        // Clear any pending interrupts
        write32(GICD_BASE + GICD_ICPENDR + (i / 32) * 4, 0xFFFFFFFF);
    }
    
    // Set all SPIs to Group 0 (secure/non-secure depends on config)
    for (uint32_t i = 32; i < num_irqs; i += 32) {
        write32(GICD_BASE + GICD_IGROUPR + (i / 32) * 4, 0);
    }
    
    // Set default priority for all SPIs (lower value = higher priority)
    // Priority 0xA0 is a reasonable default
    for (uint32_t i = 32; i < num_irqs; i += 4) {
        write32(GICD_BASE + GICD_IPRIORITYR + i, 0xA0A0A0A0);
    }
    
    // Target all SPIs to CPU 0
    for (uint32_t i = 32; i < num_irqs; i += 4) {
        write32(GICD_BASE + GICD_ITARGETSR + i, 0x01010101);
    }
    
    // Configure all SPIs as level-triggered
    for (uint32_t i = 32; i < num_irqs; i += 16) {
        write32(GICD_BASE + GICD_ICFGR + (i / 16) * 4, 0);
    }
    
    // Enable distributor (Group 0 and Group 1)
    write32(GICD_BASE + GICD_CTLR, 0x3);
}

/*
 * Initialize GIC CPU Interface (GICC)
 * Each CPU has its own interface for receiving interrupts
 */
static void init_cpu_interface() {
    // Disable CPU interface while configuring
    write32(GICC_BASE + GICC_CTLR, 0);
    
    // Set priority mask to allow all priorities
    // 0xFF means all interrupts are allowed
    write32(GICC_BASE + GICC_PMR, 0xFF);
    
    // Set binary point to 0 (all priority bits used for preemption)
    write32(GICC_BASE + GICC_BPR, 0);
    
    // Enable CPU interface (Group 0 and Group 1)
    write32(GICC_BASE + GICC_CTLR, 0x3);
}

/*
 * Initialize GIC (Distributor and CPU Interface)
 * Requirements: 3.1
 */
void init() {
    uart::puts("[gic] Initializing GICv2...\n");
    
    // Initialize distributor first
    init_distributor();
    
    // Then initialize CPU interface
    init_cpu_interface();
    
    uart::puts("[gic] GICv2 initialization complete\n");
}

/*
 * Enable a specific IRQ
 */
void enable_irq(uint32_t irq) {
    if (irq >= num_irqs) {
        return;
    }
    
    uint32_t reg_index = irq / 32;
    uint32_t bit_index = irq % 32;
    
    // Write to Set-Enable register (writing 1 enables, 0 has no effect)
    write32(GICD_BASE + GICD_ISENABLER + reg_index * 4, 1 << bit_index);
}

/*
 * Disable a specific IRQ
 */
void disable_irq(uint32_t irq) {
    if (irq >= num_irqs) {
        return;
    }
    
    uint32_t reg_index = irq / 32;
    uint32_t bit_index = irq % 32;
    
    // Write to Clear-Enable register (writing 1 disables, 0 has no effect)
    write32(GICD_BASE + GICD_ICENABLER + reg_index * 4, 1 << bit_index);
}

/*
 * Set priority for a specific IRQ (0 = highest, 255 = lowest)
 */
void set_priority(uint32_t irq, uint8_t priority) {
    if (irq >= num_irqs) {
        return;
    }
    
    uint32_t reg_index = irq / 4;
    uint32_t byte_offset = irq % 4;
    
    // Read current register value
    uint32_t reg = read32(GICD_BASE + GICD_IPRIORITYR + reg_index * 4);
    
    // Clear and set the priority byte
    reg &= ~(0xFF << (byte_offset * 8));
    reg |= (priority << (byte_offset * 8));
    
    write32(GICD_BASE + GICD_IPRIORITYR + reg_index * 4, reg);
}

/*
 * Set target CPU for a specific IRQ (bitmask)
 */
void set_target(uint32_t irq, uint8_t cpu_mask) {
    if (irq < 32 || irq >= num_irqs) {
        // SGIs and PPIs (0-31) have fixed targets
        return;
    }
    
    uint32_t reg_index = irq / 4;
    uint32_t byte_offset = irq % 4;
    
    // Read current register value
    uint32_t reg = read32(GICD_BASE + GICD_ITARGETSR + reg_index * 4);
    
    // Clear and set the target byte
    reg &= ~(0xFF << (byte_offset * 8));
    reg |= (cpu_mask << (byte_offset * 8));
    
    write32(GICD_BASE + GICD_ITARGETSR + reg_index * 4, reg);
}

/*
 * Acknowledge an interrupt (read IAR)
 * Returns the IRQ number (1023 = spurious)
 */
uint32_t acknowledge_irq() {
    return read32(GICC_BASE + GICC_IAR) & 0x3FF;
}

/*
 * Signal end of interrupt (write EOIR)
 */
void end_irq(uint32_t irq) {
    write32(GICC_BASE + GICC_EOIR, irq);
}

/*
 * Send Software Generated Interrupt (SGI)
 * SGIs are IRQs 0-15, used for inter-processor communication
 */
void send_sgi(uint32_t irq, uint8_t target_list) {
    if (irq > 15) {
        return;
    }
    
    // SGIR format:
    // [25:24] TargetListFilter: 0 = use target list
    // [23:16] CPUTargetList: bitmask of target CPUs
    // [15:4]  Reserved
    // [3:0]   SGIINTID: SGI interrupt ID (0-15)
    uint32_t sgir = (target_list << 16) | irq;
    write32(GICD_BASE + GICD_SGIR, sgir);
}

/*
 * Get number of supported IRQs
 */
uint32_t get_num_irqs() {
    return num_irqs;
}

} // namespace gic
