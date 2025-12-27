/*
 * EmberOS UART Driver Implementation
 * PL011 UART for QEMU virt machine
 * 
 * Requirements: 4.1, 4.2, 4.3, 4.4
 */

#include "uart.h"

// Signed type definitions
using int64_t = long long;

// Variadic argument support for printf
using va_list = __builtin_va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap) __builtin_va_end(ap)
#define va_arg(ap, type) __builtin_va_arg(ap, type)

namespace uart {

// PL011 UART base address for QEMU virt machine
constexpr uintptr_t UART_BASE = 0x09000000;

// PL011 Register offsets
constexpr uint32_t UART_DR   = 0x00;  // Data Register
constexpr uint32_t UART_FR   = 0x18;  // Flag Register
constexpr uint32_t UART_IBRD = 0x24;  // Integer Baud Rate Divisor
constexpr uint32_t UART_FBRD = 0x28;  // Fractional Baud Rate Divisor
constexpr uint32_t UART_LCR  = 0x2C;  // Line Control Register
constexpr uint32_t UART_CR   = 0x30;  // Control Register
constexpr uint32_t UART_IMSC = 0x38;  // Interrupt Mask Set/Clear

// Flag Register bits
constexpr uint32_t FR_RXFE = (1 << 4);  // Receive FIFO empty
constexpr uint32_t FR_TXFF = (1 << 5);  // Transmit FIFO full
constexpr uint32_t FR_RXFF = (1 << 6);  // Receive FIFO full
constexpr uint32_t FR_TXFE = (1 << 7);  // Transmit FIFO empty

// Line Control Register bits
constexpr uint32_t LCR_FEN  = (1 << 4);  // Enable FIFOs
constexpr uint32_t LCR_WLEN_8 = (3 << 5); // 8-bit word length

// Control Register bits
constexpr uint32_t CR_UARTEN = (1 << 0);  // UART enable
constexpr uint32_t CR_TXE    = (1 << 8);  // Transmit enable
constexpr uint32_t CR_RXE    = (1 << 9);  // Receive enable

// Helper functions for register access
static inline void write_reg(uint32_t offset, uint32_t value) {
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t*>(UART_BASE + offset);
    *reg = value;
}

static inline uint32_t read_reg(uint32_t offset) {
    volatile uint32_t* reg = reinterpret_cast<volatile uint32_t*>(UART_BASE + offset);
    return *reg;
}


/*
 * Initialize UART hardware
 * Requirements: 4.1
 */
void init() {
    // Disable UART before configuration
    write_reg(UART_CR, 0);
    
    // Clear all pending interrupts
    write_reg(UART_IMSC, 0);
    
    // Set baud rate (115200 with 24MHz clock)
    // Divisor = 24000000 / (16 * 115200) = 13.0208
    // Integer part = 13, Fractional part = 0.0208 * 64 = 1.33 ≈ 1
    write_reg(UART_IBRD, 13);
    write_reg(UART_FBRD, 1);
    
    // Configure line control: 8 bits, no parity, 1 stop bit, enable FIFOs
    write_reg(UART_LCR, LCR_WLEN_8 | LCR_FEN);
    
    // Enable UART, TX, and RX
    write_reg(UART_CR, CR_UARTEN | CR_TXE | CR_RXE);
}

/*
 * Write a single character to the console
 * Requirements: 4.2
 */
void putc(char c) {
    // Wait until transmit FIFO is not full
    while (read_reg(UART_FR) & FR_TXFF) {
        // Busy wait
    }
    
    // Write character to data register
    write_reg(UART_DR, static_cast<uint32_t>(c));
}

/*
 * Read a single character from the console (blocking)
 * Requirements: 4.3
 */
char getc() {
    // Wait until receive FIFO is not empty
    while (read_reg(UART_FR) & FR_RXFE) {
        // Busy wait
    }
    
    // Read character from data register
    return static_cast<char>(read_reg(UART_DR) & 0xFF);
}

/*
 * Check if input is available
 * Requirements: 4.5
 */
bool has_input() {
    // Return true if receive FIFO is not empty
    return !(read_reg(UART_FR) & FR_RXFE);
}

/*
 * Write a null-terminated string to the console
 * Requirements: 4.4
 */
void puts(const char* str) {
    while (*str) {
        // Handle newline by adding carriage return
        if (*str == '\n') {
            putc('\r');
        }
        putc(*str++);
    }
}


// Helper function to print unsigned integer in given base with optional padding
static void print_unsigned_padded(uint64_t value, int base, bool uppercase, int width, char pad_char) {
    char buffer[20];  // Enough for 64-bit number in any base
    int i = 0;
    
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    
    if (value == 0) {
        // Print padding then zero
        for (int j = 1; j < width; j++) {
            putc(pad_char);
        }
        putc('0');
        return;
    }
    
    while (value > 0) {
        buffer[i++] = digits[value % base];
        value /= base;
    }
    
    // Print padding
    for (int j = i; j < width; j++) {
        putc(pad_char);
    }
    
    // Print in reverse order
    while (i > 0) {
        putc(buffer[--i]);
    }
}

// Helper function to print unsigned integer in given base
static void print_unsigned(uint64_t value, int base, bool uppercase) {
    print_unsigned_padded(value, base, uppercase, 0, ' ');
}

// Helper function to print signed integer
static void print_signed(int64_t value) {
    if (value < 0) {
        putc('-');
        value = -value;
    }
    print_unsigned(static_cast<uint64_t>(value), 10, false);
}

/*
 * Formatted print (printf-like)
 * Supports: %d, %x, %s, %c, %p specifiers with optional width (e.g., %04x, %8d)
 * Requirements: 4.4
 */
void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            
            // Parse flags
            char pad_char = ' ';
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }
            
            // Parse width
            int width = 0;
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
            
            switch (*fmt) {
                case 'd':
                case 'i': {
                    // Signed decimal integer
                    int value = va_arg(args, int);
                    if (value < 0) {
                        putc('-');
                        value = -value;
                        if (width > 0) width--;
                    }
                    print_unsigned_padded(static_cast<uint64_t>(value), 10, false, width, pad_char);
                    break;
                }
                
                case 'u': {
                    // Unsigned decimal integer
                    unsigned int value = va_arg(args, unsigned int);
                    print_unsigned_padded(value, 10, false, width, pad_char);
                    break;
                }
                
                case 'x': {
                    // Hexadecimal (lowercase)
                    unsigned int value = va_arg(args, unsigned int);
                    print_unsigned_padded(value, 16, false, width, pad_char);
                    break;
                }
                
                case 'X': {
                    // Hexadecimal (uppercase)
                    unsigned int value = va_arg(args, unsigned int);
                    print_unsigned_padded(value, 16, true, width, pad_char);
                    break;
                }
                
                case 'p': {
                    // Pointer
                    void* ptr = va_arg(args, void*);
                    puts("0x");
                    print_unsigned_padded(reinterpret_cast<uint64_t>(ptr), 16, false, 16, '0');
                    break;
                }
                
                case 's': {
                    // String
                    const char* str = va_arg(args, const char*);
                    if (str) {
                        puts(str);
                    } else {
                        puts("(null)");
                    }
                    break;
                }
                
                case 'c': {
                    // Character
                    char c = static_cast<char>(va_arg(args, int));
                    putc(c);
                    break;
                }
                
                case '%': {
                    // Literal percent
                    putc('%');
                    break;
                }
                
                default:
                    // Unknown specifier, print as-is
                    putc('%');
                    putc(*fmt);
                    break;
            }
        } else {
            // Handle newline by adding carriage return
            if (*fmt == '\n') {
                putc('\r');
            }
            putc(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}

} // namespace uart
