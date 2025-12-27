/*
 * EmberOS Kernel Main Entry Point
 * 
 * Requirements: 1.3
 * - Initialize essential hardware (UART, interrupt controller)
 * - Basic initialization sequence
 */

#include "uart.h"
#include "memory.h"
#include "interrupts.h"
#include "timer.h"
#include "shell.h"
#include "commands.h"
#include "ramfs.h"

// Freestanding type definitions (no standard library)
using uint8_t = unsigned char;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

// External symbols from linker script
extern "C" {
    extern uint8_t __bss_start[];
    extern uint8_t __bss_end[];
    extern uint8_t __stack_top[];
    extern uint8_t __kernel_end[];
}

// Kernel version
constexpr uint32_t VERSION_MAJOR = 0;
constexpr uint32_t VERSION_MINOR = 1;
constexpr uint32_t VERSION_PATCH = 0;

// Kernel state
struct KernelState {
    bool initialized;
    uint64_t boot_time;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
};

static KernelState g_kernel = {
    .initialized = false,
    .boot_time = 0,
    .version_major = VERSION_MAJOR,
    .version_minor = VERSION_MINOR,
    .version_patch = VERSION_PATCH
};

/*
 * Kernel main entry point
 * Called from boot.S after basic CPU initialization
 * 
 * This function should never return.
 */
extern "C" void kernel_main() {
    // Mark kernel as initializing
    g_kernel.initialized = false;
    
    // Initialize UART for console output
    // Requirements: 4.1
    uart::init();
    
    // Display boot message
    // Requirements: 1.5
    uart::puts("\n");
    uart::puts("=======================================\n");
    uart::puts("         EmberOS - ARM64 Kernel        \n");
    uart::puts("=======================================\n");
    uart::printf("Version: %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    uart::puts("Platform: QEMU virt (AArch64)\n");
    uart::puts("=======================================\n");
    uart::puts("\n");
    
    /*
     * Initialize interrupt controller (GIC)
     * Requirements: 3.1, 3.2
     */
    interrupts::init();
    
    // Initialize memory manager
    // Requirements: 2.1
    // Memory starts after kernel image, ends at 128MB (QEMU virt default)
    // QEMU virt machine has RAM at 0x40000000
    constexpr uintptr_t RAM_BASE = 0x40000000;
    constexpr uintptr_t RAM_SIZE = 128 * 1024 * 1024;  // 128MB
    uintptr_t heap_start = reinterpret_cast<uintptr_t>(__kernel_end);
    uintptr_t heap_end = RAM_BASE + RAM_SIZE;
    memory::init(heap_start, heap_end);
    
    /*
     * Initialize timer
     * Requirements: 9.1
     */
    timer::init();
    
    /*
     * Initialize RAM filesystem
     */
    ramfs::init();
    
    /*
     * Enable global interrupts
     * Requirements: 3.5
     */
    interrupts::enable();
    uart::puts("[kernel] Interrupts enabled\n");
    
    /*
     * Verify timer interrupts are working
     * Wait a short time and check interrupt count
     */
    uart::puts("[kernel] Verifying timer interrupts...\n");
    uint64_t start_count = timer::get_interrupt_count();
    timer::sleep_ms(100);  // Wait 100ms (should get ~10 interrupts at 10ms interval)
    uint64_t end_count = timer::get_interrupt_count();
    uint64_t interrupts_fired = end_count - start_count;
    uart::printf("[kernel] Timer interrupts in 100ms: %d (expected ~10)\n", (uint32_t)interrupts_fired);
    
    if (interrupts_fired > 0) {
        uart::puts("[kernel] Timer interrupts: WORKING\n");
    } else {
        uart::puts("[kernel] Timer interrupts: FAILED - no interrupts detected!\n");
    }
    
    // Display uptime to verify timer is tracking time
    uart::printf("[kernel] Current uptime: %d ms\n", (uint32_t)timer::get_uptime_ms());
    
    /*
     * Initialize and run shell
     * Requirements: 5.1
     */
    shell::init();
    
    // Register built-in commands
    // Requirements: 6.1-6.13
    commands::register_all();
    
    // Mark kernel as initialized
    g_kernel.initialized = true;
    
    // Run shell (never returns)
    shell::run();
    
    // Kernel main loop (fallback if shell exits)
    // This infinite loop prevents the function from returning
    while (true) {
        // Wait for interrupt (low power state)
        asm volatile("wfe");
    }
}
