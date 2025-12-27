/*
 * EmberOS Timer Driver Implementation
 * ARM Generic Timer for QEMU virt machine
 * 
 * Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 3.5
 * 
 * The ARM Generic Timer provides:
 * - CNTPCT_EL0: Physical counter (read-only, monotonic)
 * - CNTFRQ_EL0: Counter frequency in Hz
 * - CNTP_TVAL_EL0: Timer value (countdown)
 * - CNTP_CTL_EL0: Timer control register
 * - CNTP_CVAL_EL0: Timer compare value
 */

#include "timer.h"
#include "uart.h"
#include "interrupts.h"

namespace timer {

// Timer state
static uint64_t timer_frequency = 0;      // Timer frequency in Hz
static uint64_t init_ticks = 0;           // Tick count at initialization
static uint64_t tick_interval = 0;        // Ticks per timer interrupt
static volatile uint64_t interrupt_count = 0;  // Debug: count timer interrupts

// Default timer interrupt interval (10ms for 100Hz tick rate)
constexpr uint64_t DEFAULT_INTERVAL_MS = 10;

// Callback registration
struct CallbackEntry {
    Callback callback;
    uint64_t interval_ms;
    uint64_t last_called_ms;
    bool active;
};

static CallbackEntry callbacks[MAX_CALLBACKS] = {};

/*
 * ARM Generic Timer Control Register bits (CNTP_CTL_EL0)
 */
constexpr uint64_t CNTP_CTL_ENABLE  = (1 << 0);  // Timer enable
constexpr uint64_t CNTP_CTL_IMASK   = (1 << 1);  // Interrupt mask (1 = masked)
constexpr uint64_t CNTP_CTL_ISTATUS = (1 << 2);  // Interrupt status

/*
 * Read physical counter value (CNTPCT_EL0)
 */
static inline uint64_t read_cntpct() {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

/*
 * Read counter frequency (CNTFRQ_EL0)
 */
static inline uint64_t read_cntfrq() {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

/*
 * Read timer control register (CNTP_CTL_EL0)
 */
static inline uint64_t read_cntp_ctl() {
    uint64_t val;
    asm volatile("mrs %0, cntp_ctl_el0" : "=r"(val));
    return val;
}

/*
 * Write timer control register (CNTP_CTL_EL0)
 */
static inline void write_cntp_ctl(uint64_t val) {
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(val));
}

/*
 * Write timer value register (CNTP_TVAL_EL0)
 * This sets a countdown value; when it reaches 0, an interrupt fires
 */
static inline void write_cntp_tval(uint64_t val) {
    asm volatile("msr cntp_tval_el0, %0" :: "r"(val));
}

/*
 * Read timer value register (CNTP_TVAL_EL0)
 */
static inline uint64_t read_cntp_tval() {
    uint64_t val;
    asm volatile("mrs %0, cntp_tval_el0" : "=r"(val));
    return val;
}

/*
 * Timer interrupt handler
 * Called from the interrupt subsystem when timer IRQ fires
 * Requirements: 9.5, 3.5
 */
static void timer_irq_handler(uint32_t irq) {
    (void)irq;  // IRQ number not needed
    
    // Increment interrupt counter for debugging
    interrupt_count++;
    
    // Acknowledge the timer interrupt by setting the next timeout
    // This clears the interrupt condition
    write_cntp_tval(tick_interval);
    
    // Get current uptime for callback scheduling
    uint64_t current_ms = get_uptime_ms();
    
    // Invoke registered callbacks if their interval has elapsed
    for (size_t i = 0; i < MAX_CALLBACKS; i++) {
        if (callbacks[i].active && callbacks[i].callback != nullptr) {
            uint64_t elapsed = current_ms - callbacks[i].last_called_ms;
            if (elapsed >= callbacks[i].interval_ms) {
                callbacks[i].callback();
                callbacks[i].last_called_ms = current_ms;
            }
        }
    }
}

/*
 * Initialize timer subsystem
 * Requirements: 9.1
 */
void init() {
    uart::puts("[timer] Initializing ARM Generic Timer...\n");
    
    // Read timer frequency
    timer_frequency = read_cntfrq();
    uart::printf("[timer] Timer frequency: %d Hz\n", (uint32_t)timer_frequency);
    
    // Record initial tick count
    init_ticks = read_cntpct();
    
    // Calculate tick interval for default interrupt rate
    tick_interval = (timer_frequency * DEFAULT_INTERVAL_MS) / 1000;
    uart::printf("[timer] Tick interval: %d ticks (%d ms)\n", 
                 (uint32_t)tick_interval, (uint32_t)DEFAULT_INTERVAL_MS);
    
    // Initialize callback table
    for (size_t i = 0; i < MAX_CALLBACKS; i++) {
        callbacks[i].callback = nullptr;
        callbacks[i].interval_ms = 0;
        callbacks[i].last_called_ms = 0;
        callbacks[i].active = false;
    }
    
    // Register timer interrupt handler
    // IRQ 30 is the non-secure physical timer (PPI) on QEMU virt
    interrupts::register_handler(interrupts::IRQ_TIMER, timer_irq_handler);
    
    // Enable timer IRQ in GIC
    interrupts::enable_irq(interrupts::IRQ_TIMER);
    
    // Configure and enable the timer
    // First, disable the timer while configuring
    write_cntp_ctl(0);
    
    // Set the timer value (countdown)
    write_cntp_tval(tick_interval);
    
    // Enable the timer with interrupts unmasked
    write_cntp_ctl(CNTP_CTL_ENABLE);
    
    uart::puts("[timer] Timer initialized and running\n");
}

/*
 * Get current tick count
 * Requirements: 9.2
 */
uint64_t get_ticks() {
    return read_cntpct();
}

/*
 * Get timer frequency in Hz
 */
uint64_t get_frequency() {
    return timer_frequency;
}

/*
 * Get uptime in milliseconds
 * Requirements: 9.4
 */
uint64_t get_uptime_ms() {
    if (timer_frequency == 0) {
        return 0;
    }
    
    uint64_t current_ticks = read_cntpct();
    uint64_t elapsed_ticks = current_ticks - init_ticks;
    
    // Convert ticks to milliseconds
    // Use 64-bit arithmetic carefully to avoid overflow
    // uptime_ms = (elapsed_ticks * 1000) / frequency
    // To avoid overflow, we can do: (elapsed_ticks / frequency) * 1000 + ((elapsed_ticks % frequency) * 1000) / frequency
    uint64_t seconds = elapsed_ticks / timer_frequency;
    uint64_t remainder = elapsed_ticks % timer_frequency;
    uint64_t ms = (seconds * 1000) + (remainder * 1000 / timer_frequency);
    
    return ms;
}

/*
 * Set timer to fire after specified milliseconds
 * Requirements: 9.3
 */
void set_timeout(uint64_t ms) {
    if (timer_frequency == 0) {
        return;
    }
    
    // Calculate ticks for the timeout
    uint64_t ticks = (timer_frequency * ms) / 1000;
    
    // Update the tick interval for future interrupts
    tick_interval = ticks;
    
    // Set the timer value
    write_cntp_tval(ticks);
}

/*
 * Register a periodic timer callback
 * Requirements: 9.5
 */
bool register_callback(Callback cb, uint64_t interval_ms) {
    if (cb == nullptr || interval_ms == 0) {
        return false;
    }
    
    // Find an empty slot
    for (size_t i = 0; i < MAX_CALLBACKS; i++) {
        if (!callbacks[i].active) {
            callbacks[i].callback = cb;
            callbacks[i].interval_ms = interval_ms;
            callbacks[i].last_called_ms = get_uptime_ms();
            callbacks[i].active = true;
            return true;
        }
    }
    
    // No empty slots available
    return false;
}

/*
 * Unregister a timer callback
 */
bool unregister_callback(Callback cb) {
    for (size_t i = 0; i < MAX_CALLBACKS; i++) {
        if (callbacks[i].active && callbacks[i].callback == cb) {
            callbacks[i].callback = nullptr;
            callbacks[i].interval_ms = 0;
            callbacks[i].last_called_ms = 0;
            callbacks[i].active = false;
            return true;
        }
    }
    return false;
}

/*
 * Sleep for specified milliseconds (busy wait)
 */
void sleep_ms(uint64_t ms) {
    uint64_t start = get_uptime_ms();
    while ((get_uptime_ms() - start) < ms) {
        // Busy wait - could use WFE for power savings
        asm volatile("yield");
    }
}

/*
 * Get timer interrupt count (for debugging/verification)
 */
uint64_t get_interrupt_count() {
    return interrupt_count;
}

} // namespace timer
