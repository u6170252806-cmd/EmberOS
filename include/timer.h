/*
 * EmberOS Timer Driver Header
 * ARM Generic Timer for QEMU virt machine
 * 
 * Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 3.5
 */

#ifndef EMBEROS_TIMER_H
#define EMBEROS_TIMER_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace timer {

// Maximum number of timer callbacks
constexpr size_t MAX_CALLBACKS = 8;

// Timer callback function type
using Callback = void (*)();

/*
 * Initialize timer subsystem
 * Configures the ARM generic timer and enables timer interrupts
 * Requirements: 9.1
 */
void init();

/*
 * Get current tick count (monotonic)
 * Returns the raw counter value from CNTPCT_EL0
 * Requirements: 9.2
 */
uint64_t get_ticks();

/*
 * Get timer frequency in Hz
 * Returns the value of CNTFRQ_EL0
 */
uint64_t get_frequency();

/*
 * Get uptime in milliseconds
 * Returns monotonic uptime since timer initialization
 * Requirements: 9.4
 */
uint64_t get_uptime_ms();

/*
 * Set timer to fire after specified milliseconds
 * Configures CNTP_TVAL_EL0 for the next interrupt
 * Requirements: 9.3
 */
void set_timeout(uint64_t ms);

/*
 * Register a periodic timer callback
 * The callback will be invoked at the specified interval
 * Returns true if registration succeeded, false if callback table is full
 * Requirements: 9.5
 */
bool register_callback(Callback cb, uint64_t interval_ms);

/*
 * Unregister a timer callback
 * Returns true if the callback was found and removed
 */
bool unregister_callback(Callback cb);

/*
 * Sleep for specified milliseconds (busy wait)
 * Useful for delays during initialization
 */
void sleep_ms(uint64_t ms);

/*
 * Get timer interrupt count (for debugging/verification)
 */
uint64_t get_interrupt_count();

} // namespace timer

#endif // EMBEROS_TIMER_H
