/*
 * EmberOS UART Driver Header
 * PL011 UART for QEMU virt machine
 * 
 * Requirements: 4.1, 4.2, 4.3, 4.4
 */

#ifndef EMBEROS_UART_H
#define EMBEROS_UART_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace uart {

/*
 * Initialize UART hardware
 * Configures PL011 UART at 0x09000000 for QEMU virt machine
 * Requirements: 4.1
 */
void init();

/*
 * Write a single character to the console
 * Requirements: 4.2
 */
void putc(char c);

/*
 * Read a single character from the console (blocking)
 * Requirements: 4.3
 */
char getc();

/*
 * Check if input is available
 * Requirements: 4.5
 */
bool has_input();

/*
 * Write a null-terminated string to the console
 * Requirements: 4.4
 */
void puts(const char* str);

/*
 * Formatted print (printf-like)
 * Supports: %d, %x, %s, %c, %p specifiers
 * Requirements: 4.4
 */
void printf(const char* fmt, ...);

} // namespace uart

#endif // EMBEROS_UART_H
