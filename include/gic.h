/*
 * EmberOS GIC (Generic Interrupt Controller) Driver Header
 * ARM GICv2 for QEMU virt machine
 * 
 * Requirements: 3.1
 */

#ifndef EMBEROS_GIC_H
#define EMBEROS_GIC_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace gic {

/*
 * GICv2 Base Addresses for QEMU virt machine
 * These are the standard addresses used by QEMU's virt machine
 */
constexpr uintptr_t GICD_BASE = 0x08000000;  // Distributor
constexpr uintptr_t GICC_BASE = 0x08010000;  // CPU Interface

/*
 * GIC Distributor Register Offsets (GICD)
 */
constexpr uint32_t GICD_CTLR       = 0x000;  // Distributor Control Register
constexpr uint32_t GICD_TYPER      = 0x004;  // Interrupt Controller Type Register
constexpr uint32_t GICD_IIDR       = 0x008;  // Distributor Implementer ID Register
constexpr uint32_t GICD_IGROUPR    = 0x080;  // Interrupt Group Registers (32 per reg)
constexpr uint32_t GICD_ISENABLER  = 0x100;  // Interrupt Set-Enable Registers
constexpr uint32_t GICD_ICENABLER  = 0x180;  // Interrupt Clear-Enable Registers
constexpr uint32_t GICD_ISPENDR    = 0x200;  // Interrupt Set-Pending Registers
constexpr uint32_t GICD_ICPENDR    = 0x280;  // Interrupt Clear-Pending Registers
constexpr uint32_t GICD_ISACTIVER  = 0x300;  // Interrupt Set-Active Registers
constexpr uint32_t GICD_ICACTIVER  = 0x380;  // Interrupt Clear-Active Registers
constexpr uint32_t GICD_IPRIORITYR = 0x400;  // Interrupt Priority Registers
constexpr uint32_t GICD_ITARGETSR  = 0x800;  // Interrupt Processor Targets Registers
constexpr uint32_t GICD_ICFGR      = 0xC00;  // Interrupt Configuration Registers
constexpr uint32_t GICD_SGIR       = 0xF00;  // Software Generated Interrupt Register

/*
 * GIC CPU Interface Register Offsets (GICC)
 */
constexpr uint32_t GICC_CTLR       = 0x000;  // CPU Interface Control Register
constexpr uint32_t GICC_PMR        = 0x004;  // Interrupt Priority Mask Register
constexpr uint32_t GICC_BPR        = 0x008;  // Binary Point Register
constexpr uint32_t GICC_IAR        = 0x00C;  // Interrupt Acknowledge Register
constexpr uint32_t GICC_EOIR       = 0x010;  // End of Interrupt Register
constexpr uint32_t GICC_RPR        = 0x014;  // Running Priority Register
constexpr uint32_t GICC_HPPIR      = 0x018;  // Highest Priority Pending Interrupt Register

/*
 * Initialize GIC (Distributor and CPU Interface)
 * Requirements: 3.1
 */
void init();

/*
 * Enable a specific IRQ
 */
void enable_irq(uint32_t irq);

/*
 * Disable a specific IRQ
 */
void disable_irq(uint32_t irq);

/*
 * Set priority for a specific IRQ (0 = highest, 255 = lowest)
 */
void set_priority(uint32_t irq, uint8_t priority);

/*
 * Set target CPU for a specific IRQ (bitmask)
 */
void set_target(uint32_t irq, uint8_t cpu_mask);

/*
 * Acknowledge an interrupt (read IAR)
 * Returns the IRQ number (1023 = spurious)
 */
uint32_t acknowledge_irq();

/*
 * Signal end of interrupt (write EOIR)
 */
void end_irq(uint32_t irq);

/*
 * Send Software Generated Interrupt (SGI)
 */
void send_sgi(uint32_t irq, uint8_t target_list);

/*
 * Get number of supported IRQs
 */
uint32_t get_num_irqs();

} // namespace gic

#endif // EMBEROS_GIC_H
