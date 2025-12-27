/*
 * EmberOS Built-in Shell Commands Implementation
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.10, 6.13
 */

#include "commands.h"
#include "shell.h"
#include "uart.h"
#include "memory.h"
#include "timer.h"
#include "ramfs.h"
#include "editor.h"
#include "casm/lexer.h"
#include "casm/parser.h"
#include "casm/codegen.h"

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

// Placement new operator for freestanding environment
inline void* operator new(size_t, void* ptr) noexcept { return ptr; }

namespace commands {

// ============================================================================
// String Utilities
// ============================================================================

// Parse a string to unsigned 64-bit integer (supports hex with 0x prefix)
static uint64_t parse_uint64(const char* str, bool* success) {
    uint64_t result = 0;
    *success = false;
    
    if (!str || !*str) return 0;
    
    // Check for hex prefix
    int base = 10;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        base = 16;
        str += 2;
    }
    
    while (*str) {
        char c = *str++;
        int digit;
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return 0;  // Invalid character
        }
        
        if (digit >= base) return 0;
        
        result = result * base + digit;
    }
    
    *success = true;
    return result;
}

// ============================================================================
// Basic Commands (Requirements: 6.1, 6.2, 6.3, 6.4)
// ============================================================================

// Helper to print a string padded to a fixed width
static void print_padded(const char* str, int width) {
    int len = 0;
    const char* p = str;
    while (*p++) len++;
    
    uart::puts(str);
    for (int i = len; i < width; i++) {
        uart::putc(' ');
    }
}

/*
 * help - List all available commands with descriptions
 * Requirements: 6.1
 */
void cmd_help(int argc, char* argv[]) {
    // Check for category argument
    if (argc >= 2) {
        const char* cat = argv[1];
        
        // help system
        if (cat[0] == 's' && cat[1] == 'y') {
            uart::puts("\nSystem Commands:\n");
            uart::puts("  uptime     - Display system uptime\n");
            uart::puts("  meminfo    - Display memory statistics\n");
            uart::puts("  cpuinfo    - Display CPU information\n");
            uart::puts("  date       - Display system time\n");
            uart::puts("  reboot     - Restart the system\n");
            uart::puts("  shutdown   - Halt the system\n");
            uart::puts("  ps         - List processes\n");
            uart::puts("  top        - Interactive process viewer\n");
            uart::puts("  df         - Display filesystem usage\n");
            return;
        }
        
        // help files
        if (cat[0] == 'f' && cat[1] == 'i') {
            uart::puts("\nFile Commands:\n");
            uart::puts("  ls         - List directory contents\n");
            uart::puts("  cd         - Change directory\n");
            uart::puts("  pwd        - Print working directory\n");
            uart::puts("  mkdir      - Create directory\n");
            uart::puts("  rmdir      - Remove empty directory\n");
            uart::puts("  rm         - Remove file or directory\n");
            uart::puts("  touch      - Create empty file\n");
            uart::puts("  cat        - Display file contents\n");
            uart::puts("  cp         - Copy file\n");
            uart::puts("  mv         - Move/rename file\n");
            uart::puts("  head       - Display first lines\n");
            uart::puts("  tail       - Display last lines\n");
            uart::puts("  wc         - Word/line/char count\n");
            uart::puts("  grep       - Search pattern in file\n");
            uart::puts("  find       - Search for files\n");
            uart::puts("  vi/write   - Text editors\n");
            return;
        }
        
        // help dev
        if (cat[0] == 'd' && cat[1] == 'e') {
            uart::puts("\nDeveloper Commands:\n");
            uart::puts("  casm       - C.ASM assembler/runner\n");
            uart::puts("  hexdump    - Display memory in hex\n");
            uart::puts("  xxd        - Hex dump of file\n");
            uart::puts("  regs       - Display CPU registers\n");
            return;
        }
        
        // help shell
        if (cat[0] == 's' && cat[1] == 'h') {
            uart::puts("\nShell Commands:\n");
            uart::puts("  help       - Display help\n");
            uart::puts("  clear      - Clear screen\n");
            uart::puts("  echo       - Print text\n");
            uart::puts("  history    - Command history\n");
            uart::puts("  alias      - Manage aliases\n");
            uart::puts("  env        - Environment variables\n");
            uart::puts("  export     - Set variable\n");
            uart::puts("  time       - Time a command\n");
            uart::puts("  whoami     - Current user\n");
            uart::puts("  hostname   - System hostname\n");
            return;
        }
    }
    
    // Default: show categories
    uart::puts("\nEmberOS Help - Use 'help <category>' for details\n");
    uart::puts("------------------------------------------------\n");
    uart::puts("  help system  - System & process commands\n");
    uart::puts("  help files   - File & directory commands\n");
    uart::puts("  help dev     - Developer tools (casm, hexdump)\n");
    uart::puts("  help shell   - Shell & environment commands\n");
    uart::puts("\nQuick reference:\n");
    uart::puts("  ls, cd, cat, rm, cp, mv    - File basics\n");
    uart::puts("  casm <file.asm>            - Assemble code\n");
    uart::puts("  casm run <file.bin>        - Run binary\n");
    uart::puts("\n");
}

/*
 * clear - Clear the console screen
 * Requirements: 6.2
 */
void cmd_clear(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // ANSI escape sequence to clear screen and move cursor to home
    uart::puts("\x1b[2J\x1b[H");
}

/*
 * echo - Print arguments to the console
 * Requirements: 6.3
 */
void cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            uart::putc(' ');
        }
        uart::puts(argv[i]);
    }
    uart::putc('\n');
}

/*
 * version - Display EmberOS version information
 * Requirements: 6.4
 */
void cmd_version(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uart::puts("\n");
    uart::puts("EmberOS - ARM64 Operating System\n");
    uart::puts("Version: 0.1.0\n");
    uart::puts("Platform: QEMU virt (AArch64)\n");
    uart::puts("Build: Development\n");
    uart::puts("\n");
}


// ============================================================================
// System Info Commands (Requirements: 6.5, 6.6, 6.7, 6.10)
// ============================================================================

// Helper to print a number with leading zeros
static void print_with_leading_zeros(uint32_t value, int width) {
    char buffer[12];
    int i = 0;
    
    // Convert to string (reversed)
    if (value == 0) {
        buffer[i++] = '0';
    } else {
        while (value > 0) {
            buffer[i++] = '0' + (value % 10);
            value /= 10;
        }
    }
    
    // Print leading zeros
    for (int j = i; j < width; j++) {
        uart::putc('0');
    }
    
    // Print digits in reverse
    while (i > 0) {
        uart::putc(buffer[--i]);
    }
}

/*
 * uptime - Display system uptime
 * Requirements: 6.5
 */
void cmd_uptime(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uint64_t uptime_ms = timer::get_uptime_ms();
    
    // Convert to human-readable format
    uint64_t seconds = uptime_ms / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    uint64_t days = hours / 24;
    
    uint64_t ms = uptime_ms % 1000;
    seconds = seconds % 60;
    minutes = minutes % 60;
    hours = hours % 24;
    
    uart::puts("System uptime: ");
    
    if (days > 0) {
        uart::printf("%d day", (uint32_t)days);
        if (days != 1) uart::putc('s');
        uart::puts(", ");
    }
    if (hours > 0 || days > 0) {
        uart::printf("%d hour", (uint32_t)hours);
        if (hours != 1) uart::putc('s');
        uart::puts(", ");
    }
    if (minutes > 0 || hours > 0 || days > 0) {
        uart::printf("%d minute", (uint32_t)minutes);
        if (minutes != 1) uart::putc('s');
        uart::puts(", ");
    }
    uart::printf("%d.", (uint32_t)seconds);
    print_with_leading_zeros((uint32_t)ms, 3);
    uart::puts(" seconds\n");
}

/*
 * meminfo - Display memory usage statistics
 * Requirements: 6.6
 */
void cmd_meminfo(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    memory::MemStats stats = memory::get_stats();
    
    // Calculate sizes in KB and MB
    uint64_t total_kb = (stats.total_pages * memory::PAGE_SIZE) / 1024;
    uint64_t free_kb = (stats.free_pages * memory::PAGE_SIZE) / 1024;
    uint64_t used_kb = (stats.used_pages * memory::PAGE_SIZE) / 1024;
    
    uart::puts("\nMemory Information:\n");
    uart::puts("-------------------\n");
    uart::printf("Page size:    %d bytes\n", (uint32_t)memory::PAGE_SIZE);
    uart::printf("Total pages:  %d\n", (uint32_t)stats.total_pages);
    uart::printf("Free pages:   %d\n", (uint32_t)stats.free_pages);
    uart::printf("Used pages:   %d\n", (uint32_t)stats.used_pages);
    uart::puts("\n");
    uart::printf("Total memory: %d KB (%d MB)\n", (uint32_t)total_kb, (uint32_t)(total_kb / 1024));
    uart::printf("Free memory:  %d KB (%d MB)\n", (uint32_t)free_kb, (uint32_t)(free_kb / 1024));
    uart::printf("Used memory:  %d KB (%d MB)\n", (uint32_t)used_kb, (uint32_t)(used_kb / 1024));
    
    // Calculate percentage
    if (stats.total_pages > 0) {
        uint32_t used_percent = (uint32_t)((stats.used_pages * 100) / stats.total_pages);
        uart::printf("Usage:        %d%%\n", used_percent);
    }
    uart::puts("\n");
}

/*
 * cpuinfo - Display CPU information
 * Requirements: 6.7
 */
void cmd_cpuinfo(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // Read ARM64 system registers for CPU info
    uint64_t midr_el1;
    uint64_t mpidr_el1;
    uint64_t ctr_el0;
    
    asm volatile("mrs %0, midr_el1" : "=r"(midr_el1));
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr_el1));
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr_el0));
    
    // Parse MIDR_EL1 fields
    uint32_t implementer = (midr_el1 >> 24) & 0xFF;
    uint32_t variant = (midr_el1 >> 20) & 0xF;
    uint32_t architecture = (midr_el1 >> 16) & 0xF;
    uint32_t partnum = (midr_el1 >> 4) & 0xFFF;
    uint32_t revision = midr_el1 & 0xF;
    
    // Parse MPIDR_EL1 for CPU affinity
    uint32_t aff0 = mpidr_el1 & 0xFF;
    uint32_t aff1 = (mpidr_el1 >> 8) & 0xFF;
    uint32_t aff2 = (mpidr_el1 >> 16) & 0xFF;
    uint32_t aff3 = (mpidr_el1 >> 32) & 0xFF;
    
    // Parse CTR_EL0 for cache info
    uint32_t icache_line = 4 << ((ctr_el0 >> 0) & 0xF);
    uint32_t dcache_line = 4 << ((ctr_el0 >> 16) & 0xF);
    
    uart::puts("\nCPU Information:\n");
    uart::puts("----------------\n");
    uart::puts("Architecture: AArch64 (ARM64)\n");
    
    // Decode implementer
    uart::puts("Implementer:  ");
    switch (implementer) {
        case 0x41: uart::puts("ARM Limited\n"); break;
        case 0x42: uart::puts("Broadcom\n"); break;
        case 0x43: uart::puts("Cavium\n"); break;
        case 0x44: uart::puts("DEC\n"); break;
        case 0x4E: uart::puts("NVIDIA\n"); break;
        case 0x50: uart::puts("APM\n"); break;
        case 0x51: uart::puts("Qualcomm\n"); break;
        case 0x56: uart::puts("Marvell\n"); break;
        case 0x69: uart::puts("Intel\n"); break;
        default:   uart::printf("Unknown (0x%x)\n", implementer); break;
    }
    
    uart::printf("Part number:  0x%x\n", partnum);
    uart::printf("Variant:      r%dp%d\n", variant, revision);
    uart::printf("Architecture: ARMv8.%d\n", architecture);
    uart::puts("\n");
    
    uart::puts("CPU Affinity:\n");
    uart::printf("  Aff3.Aff2.Aff1.Aff0 = %d.%d.%d.%d\n", aff3, aff2, aff1, aff0);
    uart::puts("\n");
    
    uart::puts("Cache Information:\n");
    uart::printf("  I-Cache line size: %d bytes\n", icache_line);
    uart::printf("  D-Cache line size: %d bytes\n", dcache_line);
    uart::puts("\n");
    
    // Timer frequency
    uint64_t freq = timer::get_frequency();
    uart::printf("Timer frequency: %d Hz (%d MHz)\n", (uint32_t)freq, (uint32_t)(freq / 1000000));
    uart::puts("\n");
}

/*
 * date - Display current system time
 * Requirements: 6.10
 * Note: Without RTC, we display uptime-based time since boot
 */
void cmd_date(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uint64_t uptime_ms = timer::get_uptime_ms();
    
    // Convert to time components (since boot)
    uint64_t total_seconds = uptime_ms / 1000;
    uint64_t hours = (total_seconds / 3600) % 24;
    uint64_t minutes = (total_seconds / 60) % 60;
    uint64_t seconds = total_seconds % 60;
    
    uart::puts("System time (since boot): ");
    print_with_leading_zeros((uint32_t)hours, 2);
    uart::putc(':');
    print_with_leading_zeros((uint32_t)minutes, 2);
    uart::putc(':');
    print_with_leading_zeros((uint32_t)seconds, 2);
    uart::puts("\nNote: No RTC available, showing time since boot\n");
}


// ============================================================================
// System Control Commands (Requirements: 6.8, 6.9)
// ============================================================================

/*
 * reboot - Restart the system
 * Requirements: 6.8
 */
void cmd_reboot(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uart::puts("Rebooting system...\n");
    
    // On QEMU virt machine, we can use PSCI to reboot
    // PSCI SYSTEM_RESET function ID: 0x84000009
    
    // Give time for message to be displayed
    timer::sleep_ms(100);
    
    // PSCI SYSTEM_RESET via HVC
    // Use register x0 for the function ID
    register uint64_t x0 asm("x0") = 0x84000009ULL;
    asm volatile(
        "hvc #0\n"
        : "+r"(x0)
        :
        : "x1", "x2", "x3", "memory"
    );
    
    // If HVC doesn't work, try SMC
    x0 = 0x84000009ULL;
    asm volatile(
        "smc #0\n"
        : "+r"(x0)
        :
        : "x1", "x2", "x3", "memory"
    );
    
    // If we get here, PSCI failed - just hang
    uart::puts("Reboot failed - halting\n");
    while (true) {
        asm volatile("wfe");
    }
}

/*
 * shutdown - Halt the system
 * Requirements: 6.9
 */
void cmd_shutdown(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uart::puts("Shutting down system...\n");
    
    // Give time for message to be displayed
    timer::sleep_ms(100);
    
    // PSCI SYSTEM_OFF function ID: 0x84000008
    // Use register x0 for the function ID
    register uint64_t x0 asm("x0") = 0x84000008ULL;
    asm volatile(
        "hvc #0\n"
        : "+r"(x0)
        :
        : "x1", "x2", "x3", "memory"
    );
    
    // If HVC doesn't work, try SMC
    x0 = 0x84000008ULL;
    asm volatile(
        "smc #0\n"
        : "+r"(x0)
        :
        : "x1", "x2", "x3", "memory"
    );
    
    // If we get here, PSCI failed - just halt
    uart::puts("Shutdown failed - halting\n");
    while (true) {
        asm volatile("wfe");
    }
}

// ============================================================================
// Utility Commands (Requirements: 6.13)
// ============================================================================

// Helper to print a byte as two hex digits
static void print_hex_byte(uint8_t value) {
    const char* hex = "0123456789abcdef";
    uart::putc(hex[(value >> 4) & 0xF]);
    uart::putc(hex[value & 0xF]);
}

/*
 * hexdump - Display memory contents in hexadecimal
 * Usage: hexdump <address> [length]
 * Requirements: 6.13
 */
void cmd_hexdump(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: hexdump <address> [length]\n");
        uart::puts("  address: Memory address (hex with 0x prefix or decimal)\n");
        uart::puts("  length:  Number of bytes to display (default: 256)\n");
        return;
    }
    
    bool success;
    uint64_t address = parse_uint64(argv[1], &success);
    if (!success) {
        uart::puts("Invalid address: ");
        uart::puts(argv[1]);
        uart::putc('\n');
        return;
    }
    
    uint64_t length = 256;  // Default length
    if (argc >= 3) {
        length = parse_uint64(argv[2], &success);
        if (!success) {
            uart::puts("Invalid length: ");
            uart::puts(argv[2]);
            uart::putc('\n');
            return;
        }
    }
    
    // Limit length to prevent excessive output
    if (length > 4096) {
        uart::puts("Warning: Limiting output to 4096 bytes\n");
        length = 4096;
    }
    
    uart::puts("\nMemory dump at 0x");
    uart::printf("%p", (void*)address);
    uart::puts(", ");
    uart::printf("%d", (uint32_t)length);
    uart::puts(" bytes:\n\n");
    
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(address);
    
    for (uint64_t offset = 0; offset < length; offset += 16) {
        // Print address
        uart::printf("%p", (void*)(address + offset));
        uart::puts(": ");
        
        // Print hex bytes
        for (int i = 0; i < 16; i++) {
            if (offset + i < length) {
                print_hex_byte(ptr[offset + i]);
                uart::putc(' ');
            } else {
                uart::puts("   ");
            }
            if (i == 7) uart::putc(' ');  // Extra space in middle
        }
        
        uart::puts(" |");
        
        // Print ASCII representation
        for (int i = 0; i < 16; i++) {
            if (offset + i < length) {
                uint8_t c = ptr[offset + i];
                if (c >= 0x20 && c < 0x7f) {
                    uart::putc(static_cast<char>(c));
                } else {
                    uart::putc('.');
                }
            }
        }
        
        uart::puts("|\n");
    }
    uart::puts("\n");
}

// ============================================================================
// Filesystem Commands
// ============================================================================

/*
 * ls - List directory contents
 */
void cmd_ls(int argc, char* argv[]) {
    const char* path = ".";
    if (argc > 1) {
        path = argv[1];
    }
    
    ramfs::DirIterator iter;
    if (!ramfs::dir_open(&iter, path)) {
        uart::puts("ls: cannot access '");
        uart::puts(path);
        uart::puts("': No such directory\n");
        return;
    }
    
    ramfs::FSNode* node;
    while ((node = ramfs::dir_next(&iter)) != nullptr) {
        if (node->type == ramfs::FileType::DIRECTORY) {
            uart::puts("\x1b[34m");  // Blue for directories
            uart::puts(node->name);
            uart::puts("/\x1b[0m\n");
        } else {
            uart::puts(node->name);
            uart::printf("  (%d bytes)\n", (int)node->size);
        }
    }
}

/*
 * cd - Change directory
 */
void cmd_cd(int argc, char* argv[]) {
    const char* path = "/";
    if (argc > 1) {
        path = argv[1];
    }
    
    if (!ramfs::set_cwd(path)) {
        uart::puts("cd: ");
        uart::puts(path);
        uart::puts(": No such directory\n");
    }
}

/*
 * pwd - Print working directory
 */
void cmd_pwd(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    uart::puts(ramfs::get_cwd_path());
    uart::putc('\n');
}

/*
 * mkdir - Create directory
 */
void cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: mkdir <directory>\n");
        return;
    }
    
    if (!ramfs::create_dir(argv[1])) {
        uart::puts("mkdir: cannot create directory '");
        uart::puts(argv[1]);
        uart::puts("'\n");
    }
}

/*
 * rm - Remove file or directory
 */
void cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: rm [-rf] <file|directory>\n");
        return;
    }
    
    bool recursive = false;
    int path_idx = 1;
    
    // Check for -rf flag
    if (argc > 2 && argv[1][0] == '-') {
        for (const char* p = argv[1] + 1; *p; p++) {
            if (*p == 'r' || *p == 'f') {
                recursive = true;
            }
        }
        path_idx = 2;
    }
    
    if (path_idx >= argc) {
        uart::puts("rm: missing operand\n");
        return;
    }
    
    const char* path = argv[path_idx];
    ramfs::FSNode* node = ramfs::resolve_path(path);
    
    if (!node) {
        uart::puts("rm: cannot remove '");
        uart::puts(path);
        uart::puts("': No such file or directory\n");
        return;
    }
    
    if (node->type == ramfs::FileType::DIRECTORY) {
        if (!ramfs::delete_dir(path, recursive)) {
            if (!recursive) {
                uart::puts("rm: cannot remove '");
                uart::puts(path);
                uart::puts("': Is a directory (use -rf)\n");
            } else {
                uart::puts("rm: cannot remove '");
                uart::puts(path);
                uart::puts("'\n");
            }
        }
    } else {
        if (!ramfs::delete_file(path)) {
            uart::puts("rm: cannot remove '");
            uart::puts(path);
            uart::puts("'\n");
        }
    }
}

/*
 * touch - Create empty file
 */
void cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: touch <filename>\n");
        return;
    }
    
    // Check if file exists
    ramfs::FSNode* node = ramfs::open_file(argv[1]);
    if (!node) {
        // Create new file
        node = ramfs::create_file(argv[1]);
        if (!node) {
            uart::puts("touch: cannot create '");
            uart::puts(argv[1]);
            uart::puts("'\n");
        }
    }
}

/*
 * cat - Display file contents
 */
void cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: cat <filename>\n");
        return;
    }
    
    ramfs::FSNode* file = ramfs::open_file(argv[1]);
    if (!file) {
        uart::puts("cat: ");
        uart::puts(argv[1]);
        uart::puts(": No such file\n");
        return;
    }
    
    uint8_t buffer[256];
    size_t offset = 0;
    size_t bytes_read;
    
    while ((bytes_read = ramfs::read_file(file, buffer, offset, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        uart::puts(reinterpret_cast<const char*>(buffer));
        offset += bytes_read;
    }
    
    // Ensure newline at end
    if (offset > 0) {
        uart::putc('\n');
    }
}

/*
 * vi - Text editor
 */
void cmd_vi(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: vi <filename>\n");
        return;
    }
    
    editor::Result result = editor::edit(argv[1]);
    
    if (result == editor::Result::SAVED) {
        uart::puts("File saved\n");
    } else if (result == editor::Result::QUIT) {
        uart::puts("Quit without saving\n");
    }
}

/*
 * write - Write text to a file (easier than vi for pasting)
 * Usage: write <filename>
 * Then type content, end with Ctrl+D on empty line
 */
void cmd_write(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: write <filename>\n");
        uart::puts("  Type content, then Ctrl+D on empty line to save\n");
        uart::puts("  Or Ctrl+C to cancel\n");
        return;
    }
    
    const char* filename = argv[1];
    
    // Buffer for content
    static char content[8192];
    size_t content_len = 0;
    
    uart::puts("Enter content (Ctrl+D to save, Ctrl+C to cancel):\n");
    uart::puts("---\n");
    
    char line[256];
    size_t line_len = 0;
    bool at_line_start = true;
    
    while (content_len < sizeof(content) - 2) {
        char c = uart::getc();
        
        // Ctrl+C - cancel
        if (c == 0x03) {
            uart::puts("\n[Cancelled]\n");
            return;
        }
        
        // Ctrl+D - save (only at start of line)
        if (c == 0x04 && at_line_start) {
            break;
        }
        
        // Echo character
        if (c == '\r' || c == '\n') {
            uart::putc('\n');
            // Add line to content
            for (size_t i = 0; i < line_len && content_len < sizeof(content) - 2; i++) {
                content[content_len++] = line[i];
            }
            if (content_len < sizeof(content) - 1) {
                content[content_len++] = '\n';
            }
            line_len = 0;
            at_line_start = true;
        } else if (c == '\x7f' || c == '\b') {
            // Backspace
            if (line_len > 0) {
                line_len--;
                uart::puts("\b \b");
            }
            at_line_start = (line_len == 0);
        } else if (c >= 0x20 && c < 0x7f && line_len < sizeof(line) - 1) {
            line[line_len++] = c;
            uart::putc(c);
            at_line_start = false;
        } else if (c == '\t' && line_len < sizeof(line) - 5) {
            // Tab -> 4 spaces
            for (int i = 0; i < 4 && line_len < sizeof(line) - 1; i++) {
                line[line_len++] = ' ';
                uart::putc(' ');
            }
            at_line_start = false;
        }
    }
    
    content[content_len] = '\0';
    
    uart::puts("---\n");
    
    // Create/open file
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        file = ramfs::create_file(filename);
    }
    
    if (!file) {
        uart::puts("Error: Cannot create file\n");
        return;
    }
    
    // Truncate and write
    ramfs::truncate_file(file, 0);
    size_t written = ramfs::write_file(file, reinterpret_cast<const uint8_t*>(content), 0, content_len);
    
    uart::printf("Wrote %d bytes to '%s'\n", (int)written, filename);
}

// ============================================================================
// CASM Assembler Command (Requirements: 7.11)
// ============================================================================

// Global source buffer for CASM (in BSS, zero-initialized)
static char g_casm_source[5120];  // 5KB

// Virtual framebuffer for graphics (used by VM)
static char g_framebuffer[25][81];  // 25 rows x 80 cols + null
static char g_fb_colors[25][80];    // Color attributes
static int g_fb_width = 0;
static int g_fb_height = 0;
static bool g_fb_active = false;
static int g_fb_fg = 7;  // Current foreground color
static int g_fb_bg = 0;  // Current background color

static void fb_clear() {
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            g_framebuffer[y][x] = ' ';
            g_fb_colors[y][x] = 0x70;  // white on black
        }
        g_framebuffer[y][80] = '\0';
    }
}

static void fb_plot(int x, int y, char ch) {
    if (x >= 0 && x < g_fb_width && y >= 0 && y < g_fb_height) {
        g_framebuffer[y][x] = ch ? ch : ' ';
        g_fb_colors[y][x] = (g_fb_fg & 0x7) | ((g_fb_bg & 0x7) << 4);
    }
}

static void fb_render() {
    if (!g_fb_active || g_fb_height == 0) return;
    
    int last_fg = -1, last_bg = -1;
    for (int y = 0; y < g_fb_height; y++) {
        for (int x = 0; x < g_fb_width; x++) {
            int fg = g_fb_colors[y][x] & 0x7;
            int bg = (g_fb_colors[y][x] >> 4) & 0x7;
            if (fg != last_fg || bg != last_bg) {
                uart::printf("\x1b[3%d;4%dm", fg, bg);
                last_fg = fg;
                last_bg = bg;
            }
            uart::putc(g_framebuffer[y][x]);
        }
        uart::puts("\x1b[0m\n");
        last_fg = last_bg = -1;
    }
}

// Forward declarations for casm subcommands
static void cmd_casm_run(const char* filename);
static void cmd_casm_disasm(const char* filename);
static void cmd_casm_run_debug(const char* filename);

/*
 * casm - Assemble a C.ASM source file or run a binary
 * Usage: casm <filename.asm> [-o <output>]
 *        casm run <filename.bin>
 *        casm run -d <filename.bin>  (debug mode)
 *        casm disasm <filename.bin>
 * Requirements: 7.11
 */
void cmd_casm(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("CASM - C.ASM Assembler v1.0\n\n");
        uart::puts("Usage: casm <source.asm> [-o <output.bin>]\n");
        uart::puts("       casm -r <source.asm>   (compile & run)\n");
        uart::puts("       casm run <file.bin>\n");
        uart::puts("       casm run -d <file.bin> (debug mode)\n");
        uart::puts("       casm disasm <file.bin>\n");
        return;
    }
    
    // Check for '-r' flag (compile and run)
    bool run_after_compile = false;
    const char* source_file = nullptr;
    
    if (argv[1][0] == '-' && argv[1][1] == 'r' && argv[1][2] == '\0') {
        if (argc < 3) {
            uart::puts("Usage: casm -r <source.asm>\n");
            return;
        }
        run_after_compile = true;
        source_file = argv[2];
    }
    
    // Check for 'run' subcommand
    if (argv[1][0] == 'r' && argv[1][1] == 'u' && argv[1][2] == 'n' && argv[1][3] == '\0') {
        if (argc < 3) {
            uart::puts("Usage: casm run [-d] <filename.bin>\n");
            return;
        }
        // Check for debug flag
        if (argc >= 4 && argv[2][0] == '-' && argv[2][1] == 'd') {
            cmd_casm_run_debug(argv[3]);
        } else {
            cmd_casm_run(argv[2]);
        }
        return;
    }
    
    // Check for 'disasm' subcommand
    if (argv[1][0] == 'd' && argv[1][1] == 'i' && argv[1][2] == 's') {
        if (argc < 3) {
            uart::puts("Usage: casm disasm <filename.bin>\n");
            return;
        }
        cmd_casm_disasm(argv[2]);
        return;
    }
    
    const char* input_file = run_after_compile ? source_file : argv[1];
    const char* output_file = nullptr;
    
    // Parse optional -o flag (only if not using -r)
    if (!run_after_compile) {
        for (int i = 2; i < argc - 1; i++) {
            if (argv[i][0] == '-' && argv[i][1] == 'o' && argv[i][2] == '\0') {
                output_file = argv[i + 1];
                break;
            }
        }
    }
    
    // Open and read the source file
    ramfs::FSNode* file = ramfs::open_file(input_file);
    if (!file) {
        uart::puts("casm: cannot open '");
        uart::puts(input_file);
        uart::puts("': No such file\n");
        return;
    }
    
    // Check file size
    if (file->size == 0) {
        uart::puts("casm: '");
        uart::puts(input_file);
        uart::puts("': Empty file\n");
        return;
    }
    
    if (file->size > sizeof(g_casm_source) - 1) {
        uart::puts("casm: '");
        uart::puts(input_file);
        uart::puts("': File too large (max 5KB)\n");
        return;
    }
    
    // Read file contents into global buffer
    size_t bytes_read = ramfs::read_file(file, reinterpret_cast<uint8_t*>(g_casm_source), 0, file->size);
    g_casm_source[bytes_read] = '\0';  // Null-terminate
    
    uart::puts("Assembling '");
    uart::puts(input_file);
    uart::puts("'...\n");
    
    // Create lexer (small, stays on stack)
    casm::Lexer lexer(g_casm_source);
    
    // Create parser (now smaller with reduced node pool)
    casm::Parser parser(lexer);
    
    // Parse the source
    casm::ASTNode* ast = parser.parse();
    
    if (parser.has_error()) {
        uart::puts("casm: Parse error at line ");
        uart::printf("%d", parser.get_error_line());
        uart::puts(": ");
        uart::puts(parser.get_error());
        uart::putc('\n');
        return;
    }
    
    if (!ast) {
        uart::puts("casm: Failed to parse source\n");
        return;
    }
    
    // Create code generator (now smaller with reduced buffers)
    casm::CodeGenerator codegen;
    
    if (!codegen.generate(ast)) {
        uart::puts("casm: Code generation error at line ");
        uart::printf("%d", codegen.get_error_line());
        uart::puts(": ");
        uart::puts(codegen.get_error());
        uart::putc('\n');
        return;
    }
    
    // Success - report results
    size_t code_size = codegen.get_code_size();
    int symbol_count = codegen.get_symbol_count();
    
    uart::puts("\nAssembly successful!\n");
    uart::printf("  Code size: %d bytes\n", (int)code_size);
    uart::printf("  Symbols:   %d\n", symbol_count);
    
    // List symbols if any
    if (symbol_count > 0) {
        uart::puts("\nSymbol table:\n");
        for (int i = 0; i < symbol_count; i++) {
            const casm::Symbol* sym = codegen.get_symbol(i);
            if (sym && sym->defined) {
                uart::puts("  ");
                uart::puts(sym->name);
                uart::printf(" = 0x%x", (uint32_t)sym->address);
                if (sym->is_global) {
                    uart::puts(" (global)");
                }
                uart::putc('\n');
            }
        }
    }
    
    // Write output file if requested
    if (output_file) {
        ramfs::FSNode* out = ramfs::create_file(output_file);
        if (!out) {
            uart::puts("\ncasm: Cannot create output file '");
            uart::puts(output_file);
            uart::puts("'\n");
        } else {
            size_t written = ramfs::write_file(out, codegen.get_code(), 0, code_size);
            if (written == code_size) {
                uart::puts("\nOutput written to '");
                uart::puts(output_file);
                uart::puts("'\n");
            } else {
                uart::puts("\ncasm: Error writing output file\n");
            }
        }
    }
    
    // If -r flag, run directly from memory
    if (run_after_compile) {
        uart::puts("\nRunning...\n");
        
        // Copy code to VM buffer and execute
        static uint8_t code_buffer[5120];
        for (size_t i = 0; i < sizeof(code_buffer); i++) code_buffer[i] = 0;
        
        const uint8_t* code = codegen.get_code();
        for (size_t i = 0; i < code_size && i < sizeof(code_buffer); i++) {
            code_buffer[i] = code[i];
        }
        
        // VM state
        uint64_t regs[32];
        for (int i = 0; i < 32; i++) regs[i] = 0;
        
        bool flag_n = false, flag_z = false, flag_c = false, flag_v = false;
        uint64_t pc = 0;
        bool running = true;
        int max_instructions = 10000;
        int instr_count = 0;
        size_t mem_size = sizeof(code_buffer);
        
        // Reset framebuffer
        g_fb_active = false;
        g_fb_width = 0;
        g_fb_height = 0;
        g_fb_fg = 7;
        g_fb_bg = 0;
        fb_clear();
        
        while (running && pc < code_size && instr_count < max_instructions) {
            uint32_t instr = code_buffer[pc] | (code_buffer[pc+1] << 8) |
                            (code_buffer[pc+2] << 16) | (code_buffer[pc+3] << 24);
            instr_count++;
            
            // SVC (extended opcodes)
            if ((instr & 0xFFE0001F) == 0xD4000001) {
                uint16_t imm16 = (instr >> 5) & 0xFFFF;
                switch (imm16) {
                    case 0x100: { uint64_t addr = regs[0]; while (addr < mem_size && code_buffer[addr]) uart::putc(code_buffer[addr++]); break; }
                    case 0x101: uart::putc(regs[0] & 0xFF); break;
                    case 0x102: uart::printf("%d", (int)regs[0]); break;
                    case 0x103: regs[0] = uart::getc(); break;
                    case 0x104: {
                        uint64_t buf = regs[0], max = regs[1] ? regs[1] : 64;
                        if (max > 256) max = 256;
                        uint64_t i = 0;
                        while (i < max - 1) {
                            char c = uart::getc();
                            if (c == '\r' || c == '\n') { uart::putc('\n'); break; }
                            if (c == 127 || c == 8) { if (i > 0) { i--; uart::puts("\b \b"); } continue; }
                            uart::putc(c);
                            if (buf + i < mem_size) code_buffer[buf + i] = c;
                            i++;
                        }
                        if (buf + i < mem_size) code_buffer[buf + i] = 0;
                        regs[0] = i;
                        break;
                    }
                    case 0x110: if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; } fb_clear(); break;
                    case 0x111: if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); } g_fb_fg = regs[0] & 7; g_fb_bg = regs[1] & 7; break;
                    case 0x112: if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); } fb_plot(regs[0] & 0xFF, regs[1] & 0xFF, regs[2] & 0xFF); break;
                    case 0x113: {
                        if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); }
                        int x1 = regs[0] & 0xFF, y1 = regs[1] & 0xFF, x2 = regs[2] & 0xFF, y2 = regs[3] & 0xFF;
                        char ch = (regs[4] & 0xFF) ? (regs[4] & 0xFF) : '*';
                        if (y1 == y2) { int s = (x1 < x2) ? x1 : x2, e = (x1 > x2) ? x1 : x2; for (int i = s; i <= e; i++) fb_plot(i, y1, ch); }
                        else if (x1 == x2) { int s = (y1 < y2) ? y1 : y2, e = (y1 > y2) ? y1 : y2; for (int i = s; i <= e; i++) fb_plot(x1, i, ch); }
                        break;
                    }
                    case 0x114: {
                        if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); }
                        int x = regs[0] & 0xFF, y = regs[1] & 0xFF, w = regs[2] & 0xFF, h = regs[3] & 0xFF;
                        fb_plot(x, y, '+'); for (int i = 1; i < w - 1; i++) fb_plot(x + i, y, '-'); fb_plot(x + w - 1, y, '+');
                        for (int i = 1; i < h - 1; i++) { fb_plot(x, y + i, '|'); for (int j = 1; j < w - 1; j++) fb_plot(x + j, y + i, ' '); fb_plot(x + w - 1, y + i, '|'); }
                        fb_plot(x, y + h - 1, '+'); for (int i = 1; i < w - 1; i++) fb_plot(x + i, y + h - 1, '-'); fb_plot(x + w - 1, y + h - 1, '+');
                        break;
                    }
                    case 0x115: g_fb_fg = 7; g_fb_bg = 0; break;
                    case 0x116: {
                        g_fb_width = regs[0] & 0xFF; g_fb_height = regs[1] & 0xFF;
                        if (g_fb_width < 1) g_fb_width = 40; if (g_fb_height < 1) g_fb_height = 10;
                        if (g_fb_width > 80) g_fb_width = 80; if (g_fb_height > 24) g_fb_height = 24;
                        g_fb_active = true; fb_clear();
                        break;
                    }
                    case 0x120: case 0x121: case 0x122: case 0x123: case 0x124: case 0x125: case 0x126: {
                        // File ops - extract filename
                        char fname[64]; uint64_t na = regs[0]; int j = 0;
                        while (j < 63 && na + j < mem_size && code_buffer[na + j]) { fname[j] = code_buffer[na + j]; j++; }
                        fname[j] = 0;
                        if (imm16 == 0x120) { regs[0] = ramfs::create_file(fname) ? 1 : 0; }
                        else if (imm16 == 0x121) {
                            uint64_t da = regs[1], len = regs[2];
                            ramfs::FSNode* f = ramfs::open_file(fname); if (!f) f = ramfs::create_file(fname);
                            if (f && da + len <= mem_size) { ramfs::write_file(f, &code_buffer[da], 0, len); regs[0] = len; } else regs[0] = 0;
                        }
                        else if (imm16 == 0x122) {
                            uint64_t ba = regs[1], ml = regs[2];
                            ramfs::FSNode* f = ramfs::open_file(fname);
                            if (f && ba + ml <= mem_size) regs[0] = ramfs::read_file(f, &code_buffer[ba], 0, ml); else regs[0] = 0;
                        }
                        else if (imm16 == 0x123) { regs[0] = ramfs::delete_file(fname) ? 1 : 0; }
                        else if (imm16 == 0x124 || imm16 == 0x125) {
                            char dst[64]; uint64_t da = regs[1]; j = 0;
                            while (j < 63 && da + j < mem_size && code_buffer[da + j]) { dst[j] = code_buffer[da + j]; j++; }
                            dst[j] = 0;
                            ramfs::FSNode* sf = ramfs::open_file(fname);
                            if (sf) {
                                static uint8_t cb[1024]; size_t sz = ramfs::read_file(sf, cb, 0, sizeof(cb));
                                ramfs::FSNode* df = ramfs::create_file(dst);
                                if (df) { ramfs::write_file(df, cb, 0, sz); if (imm16 == 0x125) ramfs::delete_file(fname); regs[0] = 1; }
                                else regs[0] = 0;
                            } else regs[0] = 0;
                        }
                        else if (imm16 == 0x126) { regs[0] = ramfs::open_file(fname) ? 1 : 0; }
                        break;
                    }
                    // Memory/String opcodes (0x130-0x13F)
                    case 0x105: { // prtx - print hex
                        uint64_t v = regs[0];
                        uart::printf("0x%x", (uint32_t)v);
                        break;
                    }
                    case 0x130: { // strlen
                        uint64_t addr = regs[0], len = 0;
                        while (addr + len < mem_size && code_buffer[addr + len]) len++;
                        regs[0] = len;
                        break;
                    }
                    case 0x131: { // memcpy (x0=dst, x1=src, x2=len)
                        uint64_t dst = regs[0], src = regs[1], len = regs[2];
                        for (uint64_t i = 0; i < len && dst + i < mem_size && src + i < mem_size; i++)
                            code_buffer[dst + i] = code_buffer[src + i];
                        break;
                    }
                    case 0x132: { // memset (x0=addr, x1=byte, x2=len)
                        uint64_t addr = regs[0], len = regs[2];
                        uint8_t val = regs[1] & 0xFF;
                        for (uint64_t i = 0; i < len && addr + i < mem_size; i++)
                            code_buffer[addr + i] = val;
                        break;
                    }
                    case 0x133: { // abs
                        int64_t v = (int64_t)regs[0];
                        regs[0] = (v < 0) ? -v : v;
                        break;
                    }
                    case 0x1F0: timer::sleep_ms(regs[0] & 0xFFFF); break;
                    case 0x1F1: { static uint32_t seed = 12345; seed = seed * 1103515245 + 12345; regs[0] = (seed >> 16) % (regs[0] ? regs[0] : 1); break; }
                    case 0x1F2: regs[0] = timer::get_uptime_ms(); break;
                    case 0x1FF: running = false; break;
                    default: uart::printf("\nUnknown SVC #0x%x\n", imm16); running = false; break;
                }
                pc += 4;
            }
            else if (instr == 0xD503201F) { pc += 4; }
            else if ((instr & 0xFFFFFC1F) == 0xD65F0000) { running = false; }
            else if ((instr & 0xFF800000) == 0xD2800000) { int rd = instr & 0x1F; uint16_t imm = (instr >> 5) & 0xFFFF; int hw = (instr >> 21) & 0x3; if (rd != 31) regs[rd] = (uint64_t)imm << (hw * 16); pc += 4; }
            else if ((instr & 0xFF800000) == 0x52800000) { int rd = instr & 0x1F; uint16_t imm = (instr >> 5) & 0xFFFF; int hw = (instr >> 21) & 0x3; if (rd != 31) regs[rd] = ((uint64_t)imm << (hw * 16)) & 0xFFFFFFFF; pc += 4; }
            else if ((instr & 0xFF800000) == 0x92800000) { int rd = instr & 0x1F; uint16_t imm = (instr >> 5) & 0xFFFF; int hw = (instr >> 21) & 0x3; if (rd != 31) regs[rd] = ~((uint64_t)imm << (hw * 16)); pc += 4; }
            else if ((instr & 0xFF000000) == 0x91000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; int sh = (instr >> 22) & 1; if (rd != 31) regs[rd] = regs[rn] + (sh ? (imm << 12) : imm); pc += 4; }
            else if ((instr & 0xFF000000) == 0x11000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; int sh = (instr >> 22) & 1; if (rd != 31) regs[rd] = (regs[rn] + (sh ? (imm << 12) : imm)) & 0xFFFFFFFF; pc += 4; }
            else if ((instr & 0xFF000000) == 0xD1000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; int sh = (instr >> 22) & 1; if (rd != 31) regs[rd] = regs[rn] - (sh ? (imm << 12) : imm); pc += 4; }
            else if ((instr & 0xFF000000) == 0x51000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; int sh = (instr >> 22) & 1; if (rd != 31) regs[rd] = (regs[rn] - (sh ? (imm << 12) : imm)) & 0xFFFFFFFF; pc += 4; }
            else if ((instr & 0xFC000000) == 0x14000000) { int32_t imm = instr & 0x3FFFFFF; if (imm & 0x2000000) imm |= 0xFC000000; pc = pc + (imm << 2); }
            else if ((instr & 0xFC000000) == 0x94000000) { int32_t imm = instr & 0x3FFFFFF; if (imm & 0x2000000) imm |= 0xFC000000; regs[30] = pc + 4; pc = pc + (imm << 2); }
            else if ((instr & 0xFF200000) == 0xAA000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = regs[rn] | regs[rm]; pc += 4; }
            else if ((instr & 0xFF200000) == 0x2A000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = (regs[rn] | regs[rm]) & 0xFFFFFFFF; pc += 4; }
            else if ((instr & 0xFF200000) == 0x8B000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = regs[rn] + regs[rm]; pc += 4; }
            else if ((instr & 0xFF200000) == 0x0B000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = (regs[rn] + regs[rm]) & 0xFFFFFFFF; pc += 4; }
            else if ((instr & 0xFF200000) == 0xCB000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = regs[rn] - regs[rm]; pc += 4; }
            else if ((instr & 0xFF200000) == 0x4B000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = (regs[rn] - regs[rm]) & 0xFFFFFFFF; pc += 4; }
            else if ((instr & 0xFF200000) == 0xEB000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; uint64_t o1 = regs[rn], o2 = regs[rm], r = o1 - o2; flag_n = (r >> 63) & 1; flag_z = (r == 0); flag_c = (o1 >= o2); flag_v = ((o1 ^ o2) & (o1 ^ r)) >> 63; if (rd != 31) regs[rd] = r; pc += 4; }
            else if ((instr & 0xFF200000) == 0x6B000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; uint32_t o1 = regs[rn], o2 = regs[rm], r = o1 - o2; flag_n = (r >> 31) & 1; flag_z = (r == 0); flag_c = (o1 >= o2); flag_v = ((o1 ^ o2) & (o1 ^ r)) >> 31; if (rd != 31) regs[rd] = r; pc += 4; }
            else if ((instr & 0xFF000000) == 0xF1000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; int sh = (instr >> 22) & 1; uint64_t o1 = regs[rn], o2 = sh ? (imm << 12) : imm, r = o1 - o2; flag_n = (r >> 63) & 1; flag_z = (r == 0); flag_c = (o1 >= o2); flag_v = ((o1 ^ o2) & (o1 ^ r)) >> 63; if (rd != 31) regs[rd] = r; pc += 4; }
            else if ((instr & 0xFF000000) == 0x71000000) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; int sh = (instr >> 22) & 1; uint32_t o1 = regs[rn], o2 = sh ? (imm << 12) : imm, r = o1 - o2; flag_n = (r >> 31) & 1; flag_z = (r == 0); flag_c = (o1 >= o2); flag_v = ((o1 ^ o2) & (o1 ^ r)) >> 31; if (rd != 31) regs[rd] = r; pc += 4; }
            else if ((instr & 0xFF000010) == 0x54000000) {
                int cond = instr & 0xF; int32_t imm = (instr >> 5) & 0x7FFFF; if (imm & 0x40000) imm |= 0xFFF80000;
                bool take = false;
                switch (cond) {
                    case 0: take = flag_z; break; case 1: take = !flag_z; break; case 2: take = flag_c; break; case 3: take = !flag_c; break;
                    case 4: take = flag_n; break; case 5: take = !flag_n; break; case 6: take = flag_v; break; case 7: take = !flag_v; break;
                    case 8: take = flag_c && !flag_z; break; case 9: take = !flag_c || flag_z; break;
                    case 10: take = (flag_n == flag_v); break; case 11: take = (flag_n != flag_v); break;
                    case 12: take = !flag_z && (flag_n == flag_v); break; case 13: take = flag_z || (flag_n != flag_v); break;
                    case 14: case 15: take = true; break;
                }
                if (take) pc = pc + (imm << 2); else pc += 4;
            }
            else if ((instr & 0xFFC00000) == 0xF9400000) { int rt = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; uint64_t addr = regs[rn] + (imm << 3); if (addr + 7 < mem_size) { uint64_t v = 0; for (int i = 0; i < 8; i++) v |= (uint64_t)code_buffer[addr + i] << (i * 8); if (rt != 31) regs[rt] = v; } pc += 4; }
            else if ((instr & 0xFFC00000) == 0xB9400000) { int rt = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; uint64_t addr = regs[rn] + (imm << 2); if (addr + 3 < mem_size) { uint32_t v = 0; for (int i = 0; i < 4; i++) v |= (uint32_t)code_buffer[addr + i] << (i * 8); if (rt != 31) regs[rt] = v; } pc += 4; }
            else if ((instr & 0xFFC00000) == 0x39400000) { int rt = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; uint64_t addr = regs[rn] + imm; if (addr < mem_size && rt != 31) regs[rt] = code_buffer[addr]; pc += 4; }
            else if ((instr & 0xFFC00000) == 0xF9000000) { int rt = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; uint64_t addr = regs[rn] + (imm << 3), v = regs[rt]; if (addr + 7 < mem_size) for (int i = 0; i < 8; i++) code_buffer[addr + i] = (v >> (i * 8)) & 0xFF; pc += 4; }
            else if ((instr & 0xFFC00000) == 0x39000000) { int rt = instr & 0x1F, rn = (instr >> 5) & 0x1F; uint32_t imm = (instr >> 10) & 0xFFF; uint64_t addr = regs[rn] + imm; if (addr < mem_size) code_buffer[addr] = regs[rt] & 0xFF; pc += 4; }
            else if ((instr & 0xFFE0FC00) == 0x9B007C00) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = regs[rn] * regs[rm]; pc += 4; }
            else if ((instr & 0xFFE0FC00) == 0x1B007C00) { int rd = instr & 0x1F, rn = (instr >> 5) & 0x1F, rm = (instr >> 16) & 0x1F; if (rd != 31) regs[rd] = (regs[rn] * regs[rm]) & 0xFFFFFFFF; pc += 4; }
            else { uart::printf("\nUnknown instruction 0x%08x at PC=0x%x\n", instr, (uint32_t)pc); running = false; }
        }
        
        if (g_fb_active) fb_render();
        uart::puts("\x1b[0m");
        if (instr_count >= max_instructions) uart::puts("Execution limit reached\n");
        uart::printf("Executed %d instructions\n", instr_count);
        return;
    }
    
    // Show hex dump of generated code (only when not using -r)
    if (code_size > 0) {
        uart::puts("\nGenerated code:\n");
        
        const uint8_t* code = codegen.get_code();
        for (size_t i = 0; i < code_size; i += 4) {
            if (i % 16 == 0) {
                uart::printf("  %04x: ", (uint32_t)i);
            }
            
            // Print as 32-bit instruction (little-endian)
            if (i + 3 < code_size) {
                uint32_t instr = code[i] | (code[i+1] << 8) | 
                                (code[i+2] << 16) | (code[i+3] << 24);
                uart::printf("%08x ", instr);
            }
            
            if ((i + 4) % 16 == 0 || i + 4 >= code_size) {
                uart::putc('\n');
            }
        }
    }
}

/*
 * Simple VM for executing CASM binaries
 * Interprets ARM64 instructions with extended I/O opcodes
 * 
 * Graphics use a virtual framebuffer that's rendered at the end
 * to avoid terminal scrolling issues.
 */

static void cmd_casm_run(const char* filename) {
    // Open the binary file
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        uart::puts("casm run: cannot open '");
        uart::puts(filename);
        uart::puts("': No such file\n");
        return;
    }
    
    if (file->size == 0) {
        uart::puts("casm run: '");
        uart::puts(filename);
        uart::puts("': Empty file\n");
        return;
    }
    
    if (file->size > 5120) {
        uart::puts("casm run: '");
        uart::puts(filename);
        uart::puts("': File too large (max 5KB)\n");
        return;
    }
    
    // Read binary into buffer - use full buffer as working memory
    static uint8_t code_buffer[5120];  // 5KB
    // Clear buffer first (for data area)
    for (size_t i = 0; i < sizeof(code_buffer); i++) code_buffer[i] = 0;
    size_t actual_code_size = ramfs::read_file(file, code_buffer, 0, file->size);
    size_t mem_size = sizeof(code_buffer);  // Full memory for data operations
    
    // VM state: 31 general purpose registers + SP
    uint64_t regs[32];
    for (int i = 0; i < 32; i++) regs[i] = 0;
    
    // Condition flags (NZCV)
    bool flag_n = false;  // Negative
    bool flag_z = false;  // Zero
    bool flag_c = false;  // Carry
    bool flag_v = false;  // Overflow
    
    uint64_t pc = 0;
    bool running = true;
    int max_instructions = 10000;
    int instr_count = 0;
    
    // Reset framebuffer state
    g_fb_active = false;
    g_fb_width = 0;
    g_fb_height = 0;
    g_fb_fg = 7;
    g_fb_bg = 0;
    fb_clear();
    
    while (running && pc < actual_code_size && instr_count < max_instructions) {
        uint32_t instr = code_buffer[pc] | (code_buffer[pc+1] << 8) |
                        (code_buffer[pc+2] << 16) | (code_buffer[pc+3] << 24);
        
        instr_count++;
        
        // Check for SVC (extended opcodes)
        if ((instr & 0xFFE0001F) == 0xD4000001) {
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            
            switch (imm16) {
                case 0x100: {
                    // PRT: print string
                    uint64_t addr = regs[0];
                    while (addr < mem_size && code_buffer[addr] != 0) {
                        uart::putc(static_cast<char>(code_buffer[addr]));
                        addr++;
                    }
                    break;
                }
                case 0x101:
                    // PRTC: print character
                    uart::putc(static_cast<char>(regs[0] & 0xFF));
                    break;
                case 0x102:
                    // PRTN: print number
                    uart::printf("%d", static_cast<int>(regs[0]));
                    break;
                case 0x103:
                    // INP: input character
                    regs[0] = uart::getc();
                    break;
                    
                // Graphics opcodes - draw to framebuffer
                case 0x110: {
                    // CLS: clear framebuffer (auto-init if needed)
                    if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; }
                    fb_clear();
                    break;
                }
                case 0x111: {
                    // SETC: set colors (auto-init if needed)
                    if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); }
                    g_fb_fg = regs[0] & 7;
                    g_fb_bg = regs[1] & 7;
                    break;
                }
                case 0x112: {
                    // PLOT: plot character (auto-init if needed)
                    if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); }
                    fb_plot(regs[0] & 0xFF, regs[1] & 0xFF, regs[2] & 0xFF);
                    break;
                }
                case 0x113: {
                    // LINE: draw line (auto-init if needed)
                    if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); }
                    int x1 = regs[0] & 0xFF;
                    int y1 = regs[1] & 0xFF;
                    int x2 = regs[2] & 0xFF;
                    int y2 = regs[3] & 0xFF;
                    char ch = (regs[4] & 0xFF) ? (regs[4] & 0xFF) : '*';
                    
                    if (y1 == y2) {
                        int start = (x1 < x2) ? x1 : x2;
                        int end = (x1 > x2) ? x1 : x2;
                        for (int i = start; i <= end; i++) fb_plot(i, y1, ch);
                    } else if (x1 == x2) {
                        int start = (y1 < y2) ? y1 : y2;
                        int end = (y1 > y2) ? y1 : y2;
                        for (int i = start; i <= end; i++) fb_plot(x1, i, ch);
                    }
                    break;
                }
                case 0x114: {
                    // BOX: draw box (auto-init if needed)
                    if (!g_fb_active) { g_fb_active = true; g_fb_width = 40; g_fb_height = 12; fb_clear(); }
                    int x = regs[0] & 0xFF;
                    int y = regs[1] & 0xFF;
                    int w = regs[2] & 0xFF;
                    int h = regs[3] & 0xFF;
                    
                    // Top border
                    fb_plot(x, y, '+');
                    for (int i = 1; i < w - 1; i++) fb_plot(x + i, y, '-');
                    fb_plot(x + w - 1, y, '+');
                    
                    // Sides with fill
                    for (int i = 1; i < h - 1; i++) {
                        fb_plot(x, y + i, '|');
                        for (int j = 1; j < w - 1; j++) fb_plot(x + j, y + i, ' ');
                        fb_plot(x + w - 1, y + i, '|');
                    }
                    
                    // Bottom border
                    fb_plot(x, y + h - 1, '+');
                    for (int i = 1; i < w - 1; i++) fb_plot(x + i, y + h - 1, '-');
                    fb_plot(x + w - 1, y + h - 1, '+');
                    break;
                }
                case 0x115: {
                    // RESET: reset colors
                    g_fb_fg = 7;
                    g_fb_bg = 0;
                    break;
                }
                case 0x116: {
                    // CANVAS: set up framebuffer
                    g_fb_width = regs[0] & 0xFF;
                    g_fb_height = regs[1] & 0xFF;
                    if (g_fb_width < 1) g_fb_width = 40;
                    if (g_fb_height < 1) g_fb_height = 10;
                    if (g_fb_width > 80) g_fb_width = 80;
                    if (g_fb_height > 24) g_fb_height = 24;
                    g_fb_active = true;
                    fb_clear();
                    break;
                }
                
                // System opcodes
                case 0x1F0:
                    timer::sleep_ms(regs[0] & 0xFFFF);
                    break;
                case 0x1F1: {
                    static uint32_t seed = 12345;
                    seed = seed * 1103515245 + 12345;
                    uint32_t max = regs[0] ? regs[0] : 1;
                    regs[0] = (seed >> 16) % max;
                    break;
                }
                case 0x1F2:
                    // TICK: get current time in ms
                    regs[0] = timer::get_uptime_ms();
                    break;
                    
                // Extended I/O opcodes
                case 0x104: {
                    // INPS: input string into buffer at x0, max length x1
                    // Returns actual length in x0
                    uint64_t buf_addr = regs[0];
                    uint64_t max_len = regs[1];
                    if (max_len == 0) max_len = 64;
                    if (max_len > 256) max_len = 256;
                    
                    uint64_t i = 0;
                    while (i < max_len - 1) {
                        char c = uart::getc();
                        if (c == '\r' || c == '\n') {
                            uart::putc('\n');
                            break;
                        }
                        if (c == 127 || c == 8) {  // Backspace
                            if (i > 0) {
                                i--;
                                uart::puts("\b \b");
                            }
                            continue;
                        }
                        uart::putc(c);  // Echo
                        if (buf_addr + i < mem_size) {
                            code_buffer[buf_addr + i] = c;
                        }
                        i++;
                    }
                    // Null terminate
                    if (buf_addr + i < mem_size) {
                        code_buffer[buf_addr + i] = 0;
                    }
                    regs[0] = i;
                    break;
                }
                
                // File opcodes (0x120-0x12F)
                case 0x120: {
                    // FCREAT: create file (x0=name_ptr)
                    char filename[64];
                    uint64_t name_addr = regs[0];
                    int j = 0;
                    while (j < 63 && name_addr + j < mem_size && code_buffer[name_addr + j] != 0) {
                        filename[j] = code_buffer[name_addr + j];
                        j++;
                    }
                    filename[j] = 0;
                    
                    ramfs::FSNode* f = ramfs::create_file(filename);
                    regs[0] = f ? 1 : 0;
                    break;
                }
                case 0x121: {
                    // FWRITE: write to file (x0=name_ptr, x1=data_ptr, x2=len)
                    char filename[64];
                    uint64_t name_addr = regs[0];
                    uint64_t data_addr = regs[1];
                    uint64_t len = regs[2];
                    
                    int j = 0;
                    while (j < 63 && name_addr + j < mem_size && code_buffer[name_addr + j] != 0) {
                        filename[j] = code_buffer[name_addr + j];
                        j++;
                    }
                    filename[j] = 0;
                    
                    // Get or create file
                    ramfs::FSNode* f = ramfs::open_file(filename);
                    if (!f) {
                        f = ramfs::create_file(filename);
                    }
                    if (f && data_addr + len <= mem_size) {
                        ramfs::write_file(f, &code_buffer[data_addr], 0, len);
                        regs[0] = len;
                    } else {
                        regs[0] = 0;
                    }
                    break;
                }
                case 0x122: {
                    // FREAD: read file (x0=name_ptr, x1=buf_ptr, x2=max_len) -> x0=bytes_read
                    char filename[64];
                    uint64_t name_addr = regs[0];
                    uint64_t buf_addr = regs[1];
                    uint64_t max_len = regs[2];
                    
                    int j = 0;
                    while (j < 63 && name_addr + j < mem_size && code_buffer[name_addr + j] != 0) {
                        filename[j] = code_buffer[name_addr + j];
                        j++;
                    }
                    filename[j] = 0;
                    
                    ramfs::FSNode* f = ramfs::open_file(filename);
                    if (f && buf_addr + max_len <= mem_size) {
                        size_t bytes = ramfs::read_file(f, &code_buffer[buf_addr], 0, max_len);
                        regs[0] = bytes;
                    } else {
                        regs[0] = 0;
                    }
                    break;
                }
                case 0x123: {
                    // FDEL: delete file (x0=name_ptr)
                    char filename[64];
                    uint64_t name_addr = regs[0];
                    int j = 0;
                    while (j < 63 && name_addr + j < mem_size && code_buffer[name_addr + j] != 0) {
                        filename[j] = code_buffer[name_addr + j];
                        j++;
                    }
                    filename[j] = 0;
                    
                    regs[0] = ramfs::delete_file(filename) ? 1 : 0;
                    break;
                }
                case 0x124: {
                    // FCOPY: copy file (x0=src_ptr, x1=dst_ptr)
                    char src[64], dst[64];
                    uint64_t src_addr = regs[0];
                    uint64_t dst_addr = regs[1];
                    
                    int j = 0;
                    while (j < 63 && src_addr + j < mem_size && code_buffer[src_addr + j] != 0) {
                        src[j] = code_buffer[src_addr + j];
                        j++;
                    }
                    src[j] = 0;
                    
                    j = 0;
                    while (j < 63 && dst_addr + j < mem_size && code_buffer[dst_addr + j] != 0) {
                        dst[j] = code_buffer[dst_addr + j];
                        j++;
                    }
                    dst[j] = 0;
                    
                    // Read source file
                    ramfs::FSNode* sf = ramfs::open_file(src);
                    if (sf) {
                        static uint8_t copy_buf[1024];
                        size_t sz = ramfs::read_file(sf, copy_buf, 0, sizeof(copy_buf));
                        ramfs::FSNode* df = ramfs::create_file(dst);
                        if (df) {
                            ramfs::write_file(df, copy_buf, 0, sz);
                            regs[0] = 1;
                        } else {
                            regs[0] = 0;
                        }
                    } else {
                        regs[0] = 0;
                    }
                    break;
                }
                case 0x125: {
                    // FMOVE: move/rename file (x0=src_ptr, x1=dst_ptr)
                    // Implemented as copy + delete
                    char src[64], dst[64];
                    uint64_t src_addr = regs[0];
                    uint64_t dst_addr = regs[1];
                    
                    int j = 0;
                    while (j < 63 && src_addr + j < mem_size && code_buffer[src_addr + j] != 0) {
                        src[j] = code_buffer[src_addr + j];
                        j++;
                    }
                    src[j] = 0;
                    
                    j = 0;
                    while (j < 63 && dst_addr + j < mem_size && code_buffer[dst_addr + j] != 0) {
                        dst[j] = code_buffer[dst_addr + j];
                        j++;
                    }
                    dst[j] = 0;
                    
                    // Copy then delete
                    ramfs::FSNode* sf = ramfs::open_file(src);
                    if (sf) {
                        static uint8_t move_buf[1024];
                        size_t sz = ramfs::read_file(sf, move_buf, 0, sizeof(move_buf));
                        ramfs::FSNode* df = ramfs::create_file(dst);
                        if (df) {
                            ramfs::write_file(df, move_buf, 0, sz);
                            ramfs::delete_file(src);
                            regs[0] = 1;
                        } else {
                            regs[0] = 0;
                        }
                    } else {
                        regs[0] = 0;
                    }
                    break;
                }
                case 0x126: {
                    // FEXIST: check if file exists (x0=name_ptr) -> x0=1 if exists
                    char filename[64];
                    uint64_t name_addr = regs[0];
                    int j = 0;
                    while (j < 63 && name_addr + j < mem_size && code_buffer[name_addr + j] != 0) {
                        filename[j] = code_buffer[name_addr + j];
                        j++;
                    }
                    filename[j] = 0;
                    
                    ramfs::FSNode* f = ramfs::open_file(filename);
                    regs[0] = f ? 1 : 0;
                    break;
                }
                
                // Memory/String opcodes (0x130-0x13F)
                case 0x105: {
                    // PRTX: print hex
                    uint64_t v = regs[0];
                    uart::printf("0x%x", (uint32_t)v);
                    break;
                }
                case 0x130: {
                    // STRLEN: get string length (x0=addr -> x0=len)
                    uint64_t addr = regs[0];
                    uint64_t len = 0;
                    while (addr + len < mem_size && code_buffer[addr + len] != 0) {
                        len++;
                    }
                    regs[0] = len;
                    break;
                }
                case 0x131: {
                    // MEMCPY: copy memory (x0=dst, x1=src, x2=len)
                    uint64_t dst = regs[0];
                    uint64_t src = regs[1];
                    uint64_t len = regs[2];
                    for (uint64_t i = 0; i < len && dst + i < mem_size && src + i < mem_size; i++) {
                        code_buffer[dst + i] = code_buffer[src + i];
                    }
                    break;
                }
                case 0x132: {
                    // MEMSET: fill memory (x0=addr, x1=byte, x2=len)
                    uint64_t addr = regs[0];
                    uint8_t val = regs[1] & 0xFF;
                    uint64_t len = regs[2];
                    for (uint64_t i = 0; i < len && addr + i < mem_size; i++) {
                        code_buffer[addr + i] = val;
                    }
                    break;
                }
                case 0x133: {
                    // ABS: absolute value (x0=val -> x0=|val|)
                    int64_t v = (int64_t)regs[0];
                    regs[0] = (v < 0) ? -v : v;
                    break;
                }
                
                case 0x1FF:
                    running = false;
                    break;
                default:
                    uart::printf("\nUnknown SVC #0x%x at PC=0x%x\n", imm16, (uint32_t)pc);
                    running = false;
                    break;
            }
            pc += 4;
        }
        // NOP
        else if (instr == 0xD503201F) {
            pc += 4;
        }
        // RET (stop execution)
        else if ((instr & 0xFFFFFC1F) == 0xD65F0000) {
            running = false;
        }
        // MOVZ (64-bit): 1 10 100101 hw imm16 Rd
        else if ((instr & 0xFF800000) == 0xD2800000) {
            int rd = instr & 0x1F;
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            int hw = (instr >> 21) & 0x3;
            
            uint64_t value = static_cast<uint64_t>(imm16) << (hw * 16);
            if (rd != 31) {
                regs[rd] = value;
            }
            pc += 4;
        }
        // MOVZ (32-bit): 0 10 100101 hw imm16 Rd
        else if ((instr & 0xFF800000) == 0x52800000) {
            int rd = instr & 0x1F;
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            int hw = (instr >> 21) & 0x3;
            
            uint64_t value = static_cast<uint64_t>(imm16) << (hw * 16);
            if (rd != 31) {
                regs[rd] = value & 0xFFFFFFFF;
            }
            pc += 4;
        }
        // MOVN (64-bit): 1 00 100101 hw imm16 Rd
        else if ((instr & 0xFF800000) == 0x92800000) {
            int rd = instr & 0x1F;
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            int hw = (instr >> 21) & 0x3;
            
            uint64_t value = ~(static_cast<uint64_t>(imm16) << (hw * 16));
            if (rd != 31) {
                regs[rd] = value;
            }
            pc += 4;
        }
        // ADD immediate (64-bit): 1 0 0 100010 sh imm12 Rn Rd
        else if ((instr & 0xFF000000) == 0x91000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            int sh = (instr >> 22) & 1;
            
            uint64_t operand = sh ? (imm12 << 12) : imm12;
            uint64_t result = regs[rn] + operand;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // ADD immediate (32-bit): 0 0 0 100010 sh imm12 Rn Rd
        else if ((instr & 0xFF000000) == 0x11000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            int sh = (instr >> 22) & 1;
            
            uint64_t operand = sh ? (imm12 << 12) : imm12;
            uint64_t result = (regs[rn] + operand) & 0xFFFFFFFF;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUB immediate (64-bit): 1 1 0 100010 sh imm12 Rn Rd
        else if ((instr & 0xFF000000) == 0xD1000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            int sh = (instr >> 22) & 1;
            
            uint64_t operand = sh ? (imm12 << 12) : imm12;
            uint64_t result = regs[rn] - operand;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUB immediate (32-bit): 0 1 0 100010 sh imm12 Rn Rd
        else if ((instr & 0xFF000000) == 0x51000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            int sh = (instr >> 22) & 1;
            
            uint64_t operand = sh ? (imm12 << 12) : imm12;
            uint64_t result = (regs[rn] - operand) & 0xFFFFFFFF;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // B (unconditional branch): 0 00101 imm26
        else if ((instr & 0xFC000000) == 0x14000000) {
            int32_t imm26 = instr & 0x3FFFFFF;
            // Sign extend
            if (imm26 & 0x2000000) {
                imm26 |= 0xFC000000;
            }
            pc = pc + (imm26 << 2);
        }
        // BL (branch with link): 1 00101 imm26
        else if ((instr & 0xFC000000) == 0x94000000) {
            int32_t imm26 = instr & 0x3FFFFFF;
            if (imm26 & 0x2000000) {
                imm26 |= 0xFC000000;
            }
            regs[30] = pc + 4;  // Save return address in LR
            pc = pc + (imm26 << 2);
        }
        // ORR (register, 64-bit): 1 01 01010 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0xAA000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = regs[rn] | regs[rm];
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // ORR (register, 32-bit): 0 01 01010 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0x2A000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = (regs[rn] | regs[rm]) & 0xFFFFFFFF;
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // ADD (register, 64-bit): 1 00 01011 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0x8B000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = regs[rn] + regs[rm];
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // ADD (register, 32-bit): 0 00 01011 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0x0B000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = (regs[rn] + regs[rm]) & 0xFFFFFFFF;
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUB (register, 64-bit): 1 10 01011 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0xCB000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = regs[rn] - regs[rm];
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUB (register, 32-bit): 0 10 01011 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0x4B000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = (regs[rn] - regs[rm]) & 0xFFFFFFFF;
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUBS (register, 64-bit) / CMP: 1 11 01011 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0xEB000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t op1 = regs[rn];
            uint64_t op2 = regs[rm];
            uint64_t result = op1 - op2;
            
            // Set flags
            flag_n = (result >> 63) & 1;
            flag_z = (result == 0);
            flag_c = (op1 >= op2);  // No borrow = carry set
            flag_v = ((op1 ^ op2) & (op1 ^ result)) >> 63;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUBS (register, 32-bit): 0 11 01011 00 0 Rm 000000 Rn Rd
        else if ((instr & 0xFF200000) == 0x6B000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint32_t op1 = regs[rn] & 0xFFFFFFFF;
            uint32_t op2 = regs[rm] & 0xFFFFFFFF;
            uint32_t result = op1 - op2;
            
            // Set flags
            flag_n = (result >> 31) & 1;
            flag_z = (result == 0);
            flag_c = (op1 >= op2);
            flag_v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUBS immediate (64-bit) / CMP: 1 11 100010 sh imm12 Rn Rd
        else if ((instr & 0xFF000000) == 0xF1000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            int sh = (instr >> 22) & 1;
            
            uint64_t op1 = regs[rn];
            uint64_t op2 = sh ? (imm12 << 12) : imm12;
            uint64_t result = op1 - op2;
            
            // Set flags
            flag_n = (result >> 63) & 1;
            flag_z = (result == 0);
            flag_c = (op1 >= op2);
            flag_v = ((op1 ^ op2) & (op1 ^ result)) >> 63;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // SUBS immediate (32-bit): 0 11 100010 sh imm12 Rn Rd
        else if ((instr & 0xFF000000) == 0x71000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            int sh = (instr >> 22) & 1;
            
            uint32_t op1 = regs[rn] & 0xFFFFFFFF;
            uint32_t op2 = sh ? (imm12 << 12) : imm12;
            uint32_t result = op1 - op2;
            
            // Set flags
            flag_n = (result >> 31) & 1;
            flag_z = (result == 0);
            flag_c = (op1 >= op2);
            flag_v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // B.cond (conditional branch): 0101010 0 imm19 0 cond
        else if ((instr & 0xFF000010) == 0x54000000) {
            int cond = instr & 0xF;
            int32_t imm19 = (instr >> 5) & 0x7FFFF;
            // Sign extend
            if (imm19 & 0x40000) {
                imm19 |= 0xFFF80000;
            }
            
            // Evaluate condition
            bool take_branch = false;
            switch (cond) {
                case 0:  take_branch = flag_z; break;                    // EQ: Z==1
                case 1:  take_branch = !flag_z; break;                   // NE: Z==0
                case 2:  take_branch = flag_c; break;                    // CS/HS: C==1
                case 3:  take_branch = !flag_c; break;                   // CC/LO: C==0
                case 4:  take_branch = flag_n; break;                    // MI: N==1
                case 5:  take_branch = !flag_n; break;                   // PL: N==0
                case 6:  take_branch = flag_v; break;                    // VS: V==1
                case 7:  take_branch = !flag_v; break;                   // VC: V==0
                case 8:  take_branch = flag_c && !flag_z; break;         // HI: C==1 && Z==0
                case 9:  take_branch = !flag_c || flag_z; break;         // LS: C==0 || Z==1
                case 10: take_branch = (flag_n == flag_v); break;        // GE: N==V
                case 11: take_branch = (flag_n != flag_v); break;        // LT: N!=V
                case 12: take_branch = !flag_z && (flag_n == flag_v); break; // GT: Z==0 && N==V
                case 13: take_branch = flag_z || (flag_n != flag_v); break;  // LE: Z==1 || N!=V
                case 14: take_branch = true; break;                      // AL: always
                case 15: take_branch = true; break;                      // NV: always (same as AL)
            }
            
            if (take_branch) {
                pc = pc + (imm19 << 2);
            } else {
                pc += 4;
            }
        }
        // LDR (unsigned offset, 64-bit): 11 111 0 01 01 imm12 Rn Rt
        else if ((instr & 0xFFC00000) == 0xF9400000) {
            int rt = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            
            uint64_t addr = regs[rn] + (imm12 << 3);
            if (addr + 7 < mem_size) {
                uint64_t value = 0;
                for (int i = 0; i < 8; i++) {
                    value |= static_cast<uint64_t>(code_buffer[addr + i]) << (i * 8);
                }
                if (rt != 31) {
                    regs[rt] = value;
                }
            }
            pc += 4;
        }
        // LDR (unsigned offset, 32-bit): 10 111 0 01 01 imm12 Rn Rt
        else if ((instr & 0xFFC00000) == 0xB9400000) {
            int rt = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            
            uint64_t addr = regs[rn] + (imm12 << 2);
            if (addr + 3 < mem_size) {
                uint32_t value = 0;
                for (int i = 0; i < 4; i++) {
                    value |= static_cast<uint32_t>(code_buffer[addr + i]) << (i * 8);
                }
                if (rt != 31) {
                    regs[rt] = value;
                }
            }
            pc += 4;
        }
        // LDRB (unsigned offset): 00 111 0 01 01 imm12 Rn Rt
        else if ((instr & 0xFFC00000) == 0x39400000) {
            int rt = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            
            uint64_t addr = regs[rn] + imm12;
            if (addr < mem_size) {
                if (rt != 31) {
                    regs[rt] = code_buffer[addr];
                }
            }
            pc += 4;
        }
        // STR (unsigned offset, 64-bit): 11 111 0 01 00 imm12 Rn Rt
        else if ((instr & 0xFFC00000) == 0xF9000000) {
            int rt = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            
            uint64_t addr = regs[rn] + (imm12 << 3);
            uint64_t value = regs[rt];
            if (addr + 7 < mem_size) {
                for (int i = 0; i < 8; i++) {
                    code_buffer[addr + i] = (value >> (i * 8)) & 0xFF;
                }
            }
            pc += 4;
        }
        // STRB (unsigned offset): 00 111 0 01 00 imm12 Rn Rt
        else if ((instr & 0xFFC00000) == 0x39000000) {
            int rt = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            uint32_t imm12 = (instr >> 10) & 0xFFF;
            
            uint64_t addr = regs[rn] + imm12;
            if (addr < mem_size) {
                code_buffer[addr] = regs[rt] & 0xFF;
            }
            pc += 4;
        }
        // MUL (64-bit): 1 00 11011 000 Rm 0 11111 Rn Rd (MADD with Ra=XZR)
        else if ((instr & 0xFFE0FC00) == 0x9B007C00) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = regs[rn] * regs[rm];
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // MUL (32-bit): 0 00 11011 000 Rm 0 11111 Rn Rd
        else if ((instr & 0xFFE0FC00) == 0x1B007C00) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            
            uint64_t result = (regs[rn] * regs[rm]) & 0xFFFFFFFF;
            if (rd != 31) {
                regs[rd] = result;
            }
            pc += 4;
        }
        // Unknown instruction
        else {
            uart::printf("\nUnknown instruction 0x%08x at PC=0x%x\n", instr, (uint32_t)pc);
            running = false;
        }
    }
    
    // Render framebuffer if graphics were used
    if (g_fb_active) {
        fb_render();
    }
    
    // Reset terminal
    uart::puts("\x1b[0m");
    
    if (instr_count >= max_instructions) {
        uart::puts("Execution limit reached (possible infinite loop)\n");
    }
    
    uart::printf("Executed %d instructions\n", instr_count);
}

// ============================================================================
// New Shell Commands: history, alias, env, time, whoami, etc.
// ============================================================================

/*
 * history - Display command history
 */
void cmd_history(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    int count = shell::get_history_count();
    if (count == 0) {
        uart::puts("No commands in history\n");
        return;
    }
    
    for (int i = 0; i < count; i++) {
        const char* entry = shell::get_history_entry(i);
        if (entry) {
            uart::printf("  %d  %s\n", i + 1, entry);
        }
    }
}

/*
 * alias - Manage command aliases
 */
void cmd_alias(int argc, char* argv[]) {
    if (argc == 1) {
        // List all aliases
        uart::puts("Usage: alias <name>=<value>\n");
        uart::puts("       alias <name>        (show alias)\n");
        uart::puts("       unalias <name>      (remove alias)\n");
        return;
    }
    
    // Check for name=value format
    char* eq = nullptr;
    for (char* p = argv[1]; *p; p++) {
        if (*p == '=') {
            eq = p;
            break;
        }
    }
    
    if (eq) {
        // Set alias
        *eq = '\0';
        const char* name = argv[1];
        const char* value = eq + 1;
        
        if (shell::add_alias(name, value)) {
            uart::printf("alias %s='%s'\n", name, value);
        } else {
            uart::puts("Error: Too many aliases\n");
        }
    } else {
        // Show alias
        const char* value = shell::get_alias(argv[1]);
        if (value) {
            uart::printf("alias %s='%s'\n", argv[1], value);
        } else {
            uart::printf("alias: %s: not found\n", argv[1]);
        }
    }
}

/*
 * unalias - Remove an alias
 */
void cmd_unalias(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: unalias <name>\n");
        return;
    }
    
    if (shell::remove_alias(argv[1])) {
        uart::printf("Removed alias '%s'\n", argv[1]);
    } else {
        uart::printf("unalias: %s: not found\n", argv[1]);
    }
}

/*
 * env - Display environment variables
 */
void cmd_env(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    int count = shell::get_env_count();
    for (int i = 0; i < count; i++) {
        const char* name = shell::get_env_name(i);
        const char* value = shell::get_env_value(i);
        if (name && value) {
            uart::printf("%s=%s\n", name, value);
        }
    }
}

/*
 * export - Set environment variable
 */
void cmd_export(int argc, char* argv[]) {
    if (argc < 2) {
        // Same as env
        cmd_env(argc, argv);
        return;
    }
    
    // Parse NAME=VALUE
    char* eq = nullptr;
    for (char* p = argv[1]; *p; p++) {
        if (*p == '=') {
            eq = p;
            break;
        }
    }
    
    if (eq) {
        *eq = '\0';
        const char* name = argv[1];
        const char* value = eq + 1;
        
        if (shell::set_env(name, value)) {
            uart::printf("%s=%s\n", name, value);
        } else {
            uart::puts("Error: Too many environment variables\n");
        }
    } else {
        // Just show the variable
        const char* value = shell::get_env(argv[1]);
        if (value) {
            uart::printf("%s=%s\n", argv[1], value);
        } else {
            uart::printf("%s: not set\n", argv[1]);
        }
    }
}

/*
 * unset - Remove environment variable
 */
void cmd_unset(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: unset <name>\n");
        return;
    }
    
    if (shell::unset_env(argv[1])) {
        uart::printf("Unset %s\n", argv[1]);
    } else {
        uart::printf("unset: %s: not found\n", argv[1]);
    }
}

/*
 * time - Time command execution
 */
void cmd_time(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: time <command> [args...]\n");
        return;
    }
    
    // Get start time
    uint64_t start = timer::get_uptime_ms();
    
    // Build command line
    static char cmdline[256];
    size_t pos = 0;
    for (int i = 1; i < argc && pos < sizeof(cmdline) - 2; i++) {
        if (i > 1) cmdline[pos++] = ' ';
        const char* arg = argv[i];
        while (*arg && pos < sizeof(cmdline) - 1) {
            cmdline[pos++] = *arg++;
        }
    }
    cmdline[pos] = '\0';
    
    // Parse and execute
    char* sub_argv[16];
    int sub_argc = shell::parse_command(cmdline, sub_argv, 16);
    
    if (sub_argc > 0) {
        shell::CommandHandler handler = shell::lookup_command(sub_argv[0]);
        if (handler) {
            handler(sub_argc, sub_argv);
        } else {
            uart::printf("time: %s: command not found\n", sub_argv[0]);
        }
    }
    
    // Get end time
    uint64_t end = timer::get_uptime_ms();
    uint64_t elapsed = end - start;
    
    uart::printf("\nreal\t%d.%03ds\n", (int)(elapsed / 1000), (int)(elapsed % 1000));
}

/*
 * whoami - Display current user
 */
void cmd_whoami(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    const char* user = shell::get_env("USER");
    uart::puts(user ? user : "root");
    uart::putc('\n');
}

/*
 * hostname - Display or set hostname
 */
void cmd_hostname(int argc, char* argv[]) {
    if (argc > 1) {
        shell::set_env("HOSTNAME", argv[1]);
    }
    
    const char* hostname = shell::get_env("HOSTNAME");
    uart::puts(hostname ? hostname : "ember");
    uart::putc('\n');
}

/*
 * head - Display first lines of a file
 */
void cmd_head(int argc, char* argv[]) {
    int num_lines = 10;
    const char* filename = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && argv[i][2] == '\0') {
            if (i + 1 < argc) {
                num_lines = 0;
                for (const char* p = argv[++i]; *p >= '0' && *p <= '9'; p++) {
                    num_lines = num_lines * 10 + (*p - '0');
                }
            }
        } else if (argv[i][0] == '-') {
            // -N format
            num_lines = 0;
            for (const char* p = argv[i] + 1; *p >= '0' && *p <= '9'; p++) {
                num_lines = num_lines * 10 + (*p - '0');
            }
        } else {
            filename = argv[i];
        }
    }
    
    if (!filename) {
        uart::puts("Usage: head [-n lines] <file>\n");
        return;
    }
    
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        uart::printf("head: %s: No such file\n", filename);
        return;
    }
    
    uint8_t buffer[256];
    size_t offset = 0;
    int lines_printed = 0;
    
    while (lines_printed < num_lines) {
        size_t bytes_read = ramfs::read_file(file, buffer, offset, sizeof(buffer) - 1);
        if (bytes_read == 0) break;
        
        buffer[bytes_read] = '\0';
        
        for (size_t i = 0; i < bytes_read && lines_printed < num_lines; i++) {
            uart::putc(buffer[i]);
            if (buffer[i] == '\n') {
                lines_printed++;
            }
        }
        
        offset += bytes_read;
    }
}

/*
 * tail - Display last lines of a file
 */
void cmd_tail(int argc, char* argv[]) {
    int num_lines = 10;
    const char* filename = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'n' && argv[i][2] == '\0') {
            if (i + 1 < argc) {
                num_lines = 0;
                for (const char* p = argv[++i]; *p >= '0' && *p <= '9'; p++) {
                    num_lines = num_lines * 10 + (*p - '0');
                }
            }
        } else if (argv[i][0] == '-') {
            num_lines = 0;
            for (const char* p = argv[i] + 1; *p >= '0' && *p <= '9'; p++) {
                num_lines = num_lines * 10 + (*p - '0');
            }
        } else {
            filename = argv[i];
        }
    }
    
    if (!filename) {
        uart::puts("Usage: tail [-n lines] <file>\n");
        return;
    }
    
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        uart::printf("tail: %s: No such file\n", filename);
        return;
    }
    
    // Read entire file and count lines
    static uint8_t buffer[8192];
    size_t size = ramfs::read_file(file, buffer, 0, sizeof(buffer) - 1);
    buffer[size] = '\0';
    
    // Count total lines
    int total_lines = 0;
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] == '\n') total_lines++;
    }
    
    // Find start position
    int skip_lines = total_lines - num_lines;
    if (skip_lines < 0) skip_lines = 0;
    
    int current_line = 0;
    for (size_t i = 0; i < size; i++) {
        if (current_line >= skip_lines) {
            uart::putc(buffer[i]);
        }
        if (buffer[i] == '\n') {
            current_line++;
        }
    }
}

/*
 * wc - Word, line, character count
 */
void cmd_wc(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: wc <file>\n");
        return;
    }
    
    ramfs::FSNode* file = ramfs::open_file(argv[1]);
    if (!file) {
        uart::printf("wc: %s: No such file\n", argv[1]);
        return;
    }
    
    static uint8_t buffer[4096];
    size_t size = ramfs::read_file(file, buffer, 0, sizeof(buffer));
    
    int lines = 0, words = 0, chars = (int)size;
    bool in_word = false;
    
    for (size_t i = 0; i < size; i++) {
        if (buffer[i] == '\n') lines++;
        
        bool is_space = (buffer[i] == ' ' || buffer[i] == '\t' || 
                        buffer[i] == '\n' || buffer[i] == '\r');
        if (is_space) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }
    
    uart::printf("  %d  %d  %d %s\n", lines, words, chars, argv[1]);
}

/*
 * grep - Search for pattern in file
 */
void cmd_grep(int argc, char* argv[]) {
    if (argc < 3) {
        uart::puts("Usage: grep <pattern> <file>\n");
        return;
    }
    
    const char* pattern = argv[1];
    const char* filename = argv[2];
    
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        uart::printf("grep: %s: No such file\n", filename);
        return;
    }
    
    static uint8_t buffer[8192];
    size_t size = ramfs::read_file(file, buffer, 0, sizeof(buffer) - 1);
    buffer[size] = '\0';
    
    // Simple line-by-line search
    char* line_start = reinterpret_cast<char*>(buffer);
    int line_num = 1;
    
    while (*line_start) {
        // Find end of line
        char* line_end = line_start;
        while (*line_end && *line_end != '\n') line_end++;
        
        char saved = *line_end;
        *line_end = '\0';
        
        // Simple substring search
        bool found = false;
        for (char* p = line_start; *p && !found; p++) {
            const char* pp = p;
            const char* pat = pattern;
            while (*pp && *pat && *pp == *pat) {
                pp++;
                pat++;
            }
            if (*pat == '\0') found = true;
        }
        
        if (found) {
            uart::printf("%d: %s\n", line_num, line_start);
        }
        
        *line_end = saved;
        line_start = (*line_end) ? line_end + 1 : line_end;
        line_num++;
    }
}

/*
 * cp - Copy file
 */
void cmd_cp(int argc, char* argv[]) {
    if (argc < 3) {
        uart::puts("Usage: cp <source> <dest>\n");
        return;
    }
    
    ramfs::FSNode* src = ramfs::open_file(argv[1]);
    if (!src) {
        uart::printf("cp: %s: No such file\n", argv[1]);
        return;
    }
    
    // Read source
    static uint8_t buffer[8192];
    size_t size = ramfs::read_file(src, buffer, 0, sizeof(buffer));
    
    // Create/open destination
    ramfs::FSNode* dst = ramfs::open_file(argv[2]);
    if (!dst) {
        dst = ramfs::create_file(argv[2]);
    }
    
    if (!dst) {
        uart::printf("cp: cannot create '%s'\n", argv[2]);
        return;
    }
    
    ramfs::truncate_file(dst, 0);
    ramfs::write_file(dst, buffer, 0, size);
    
    uart::printf("'%s' -> '%s'\n", argv[1], argv[2]);
}

/*
 * mv - Move/rename file
 */
void cmd_mv(int argc, char* argv[]) {
    if (argc < 3) {
        uart::puts("Usage: mv <source> <dest>\n");
        return;
    }
    
    // Copy then delete
    ramfs::FSNode* src = ramfs::open_file(argv[1]);
    if (!src) {
        uart::printf("mv: %s: No such file\n", argv[1]);
        return;
    }
    
    static uint8_t buffer[8192];
    size_t size = ramfs::read_file(src, buffer, 0, sizeof(buffer));
    
    ramfs::FSNode* dst = ramfs::open_file(argv[2]);
    if (!dst) {
        dst = ramfs::create_file(argv[2]);
    }
    
    if (!dst) {
        uart::printf("mv: cannot create '%s'\n", argv[2]);
        return;
    }
    
    ramfs::truncate_file(dst, 0);
    ramfs::write_file(dst, buffer, 0, size);
    ramfs::delete_file(argv[1]);
    
    uart::printf("'%s' -> '%s'\n", argv[1], argv[2]);
}

/*
 * df - Display filesystem usage
 */
void cmd_df(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    ramfs::FSStats stats = ramfs::get_stats();
    
    uart::puts("Filesystem      Size    Used    Avail   Use%\n");
    uart::printf("ramfs           %dK   %dK   %dK    %d%%\n",
        (int)(stats.total_bytes / 1024),
        (int)(stats.used_bytes / 1024),
        (int)((stats.total_bytes - stats.used_bytes) / 1024),
        stats.total_bytes ? (int)(stats.used_bytes * 100 / stats.total_bytes) : 0);
    uart::printf("Files: %d/%d\n", (int)stats.used_nodes, (int)stats.total_nodes);
}

/*
 * rmdir - Remove empty directory
 */
void cmd_rmdir(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: rmdir <directory>\n");
        return;
    }
    
    if (ramfs::delete_dir(argv[1], false)) {
        // Success, silent
    } else {
        uart::printf("rmdir: %s: Not empty or not a directory\n", argv[1]);
    }
}

/*
 * find - Search for files by name pattern
 */
static void find_recursive(ramfs::FSNode* dir, const char* pattern, const char* base_path) {
    ramfs::DirIterator iter;
    char path[128];
    
    // Build path string
    int len = 0;
    const char* p = base_path;
    while (*p && len < 126) path[len++] = *p++;
    path[len] = '\0';
    
    if (!ramfs::dir_open(&iter, path[0] ? path : "/")) return;
    
    ramfs::FSNode* node;
    while ((node = ramfs::dir_next(&iter)) != nullptr) {
        // Skip . and ..
        if (node->name[0] == '.') continue;
        
        // Check if name contains pattern (simple substring match)
        bool match = false;
        if (pattern[0] == '\0') {
            match = true;  // Empty pattern matches all
        } else {
            for (const char* n = node->name; *n && !match; n++) {
                const char* nn = n;
                const char* pp = pattern;
                while (*nn && *pp && *nn == *pp) { nn++; pp++; }
                if (*pp == '\0') match = true;
            }
        }
        
        // Build full path for this node
        char full_path[128];
        int fp_len = 0;
        p = path;
        while (*p && fp_len < 120) full_path[fp_len++] = *p++;
        if (fp_len > 0 && full_path[fp_len-1] != '/') full_path[fp_len++] = '/';
        p = node->name;
        while (*p && fp_len < 126) full_path[fp_len++] = *p++;
        full_path[fp_len] = '\0';
        
        if (match) {
            uart::puts(full_path);
            if (node->type == ramfs::FileType::DIRECTORY) uart::putc('/');
            uart::putc('\n');
        }
        
        // Recurse into directories
        if (node->type == ramfs::FileType::DIRECTORY) {
            find_recursive(node, pattern, full_path);
        }
    }
}

void cmd_find(int argc, char* argv[]) {
    const char* path = "/";
    const char* pattern = "";
    
    if (argc >= 2) path = argv[1];
    if (argc >= 3) pattern = argv[2];
    
    if (argc < 2) {
        uart::puts("Usage: find <path> [pattern]\n");
        uart::puts("  find /         - List all files\n");
        uart::puts("  find / .asm    - Find files containing '.asm'\n");
        return;
    }
    
    ramfs::FSNode* start = ramfs::resolve_path(path);
    if (!start) {
        uart::printf("find: %s: No such directory\n", path);
        return;
    }
    
    if (start->type != ramfs::FileType::DIRECTORY) {
        // Single file - check if matches
        uart::puts(path);
        uart::putc('\n');
        return;
    }
    
    find_recursive(start, pattern, path);
}

/*
 * xxd - Hex dump of file
 */
void cmd_xxd(int argc, char* argv[]) {
    if (argc < 2) {
        uart::puts("Usage: xxd <file>\n");
        return;
    }
    
    ramfs::FSNode* file = ramfs::open_file(argv[1]);
    if (!file) {
        uart::printf("xxd: %s: No such file\n", argv[1]);
        return;
    }
    
    static uint8_t buffer[512];
    size_t size = ramfs::read_file(file, buffer, 0, sizeof(buffer));
    
    for (size_t i = 0; i < size; i += 16) {
        uart::printf("%08x: ", (uint32_t)i);
        
        // Hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                uart::printf("%02x", buffer[i + j]);
            } else {
                uart::puts("  ");
            }
            if (j == 7) uart::putc(' ');
            uart::putc(' ');
        }
        
        // ASCII
        uart::puts(" ");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            char c = buffer[i + j];
            uart::putc((c >= 32 && c < 127) ? c : '.');
        }
        uart::putc('\n');
    }
    
    if (file->size > sizeof(buffer)) {
        uart::printf("... (%d more bytes)\n", (int)(file->size - sizeof(buffer)));
    }
}

/*
 * regs - Display CPU register info (simulated)
 */
void cmd_regs(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // Read some system registers
    uint64_t mpidr, midr, ctr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    asm volatile("mrs %0, midr_el1" : "=r"(midr));
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
    
    uart::puts("CPU Registers:\n");
    uart::printf("  MPIDR_EL1: 0x%08x%08x\n", (uint32_t)(mpidr >> 32), (uint32_t)mpidr);
    uart::printf("  MIDR_EL1:  0x%08x%08x\n", (uint32_t)(midr >> 32), (uint32_t)midr);
    uart::printf("  CTR_EL0:   0x%08x%08x\n", (uint32_t)(ctr >> 32), (uint32_t)ctr);
    
    // Decode MIDR
    int impl = (midr >> 24) & 0xFF;
    int variant = (midr >> 20) & 0xF;
    int partnum = (midr >> 4) & 0xFFF;
    int rev = midr & 0xF;
    
    uart::puts("\nCPU Info:\n");
    uart::printf("  Implementer: 0x%02x", impl);
    if (impl == 0x41) uart::puts(" (ARM)");
    uart::putc('\n');
    uart::printf("  Part: 0x%03x r%dp%d\n", partnum, variant, rev);
}

// ============================================================================
// CASM Disassembler
// ============================================================================

static const char* disasm_instruction(uint32_t instr, char* buf, size_t buf_size) {
    // NOP
    if (instr == 0xD503201F) {
        return "nop";
    }
    
    // RET
    if ((instr & 0xFFFFFC1F) == 0xD65F0000) {
        int rn = (instr >> 5) & 0x1F;
        if (rn == 30) return "ret";
        uart::printf("ret x%d", rn);
        return buf;
    }
    
    // SVC (including extended opcodes)
    if ((instr & 0xFFE0001F) == 0xD4000001) {
        uint16_t imm = (instr >> 5) & 0xFFFF;
        switch (imm) {
            case 0x100: return "prt";
            case 0x101: return "prtc";
            case 0x102: return "prtn";
            case 0x103: return "inp";
            case 0x104: return "inps";
            case 0x105: return "prtx";
            case 0x110: return "cls";
            case 0x111: return "setc";
            case 0x112: return "plot";
            case 0x113: return "line";
            case 0x114: return "box";
            case 0x115: return "reset";
            case 0x116: return "canvas";
            case 0x120: return "fcreat";
            case 0x121: return "fwrite";
            case 0x122: return "fread";
            case 0x123: return "fdel";
            case 0x124: return "fcopy";
            case 0x125: return "fmove";
            case 0x126: return "fexist";
            case 0x130: return "strlen";
            case 0x131: return "memcpy";
            case 0x132: return "memset";
            case 0x133: return "abs";
            case 0x1F0: return "sleep";
            case 0x1F1: return "rnd";
            case 0x1F2: return "tick";
            case 0x1FF: return "halt";
        }
        uart::printf("svc #0x%x", imm);
        return "";
    }
    
    // MOVZ 64-bit
    if ((instr & 0xFF800000) == 0xD2800000) {
        int rd = instr & 0x1F;
        uint16_t imm16 = (instr >> 5) & 0xFFFF;
        int hw = (instr >> 21) & 0x3;
        uint64_t value = static_cast<uint64_t>(imm16) << (hw * 16);
        (void)buf_size;
        // Simplified output
        uart::printf("mov x%d, #%d", rd, (int)value);
        return "";
    }
    
    // MOVZ 32-bit
    if ((instr & 0xFF800000) == 0x52800000) {
        int rd = instr & 0x1F;
        uint16_t imm16 = (instr >> 5) & 0xFFFF;
        uart::printf("mov w%d, #%d", rd, imm16);
        return "";
    }
    
    // ADD immediate 64-bit
    if ((instr & 0xFF000000) == 0x91000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        uart::printf("add x%d, x%d, #%d", rd, rn, imm12);
        return "";
    }
    
    // ADD register 64-bit
    if ((instr & 0xFF200000) == 0x8B000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("add x%d, x%d, x%d", rd, rn, rm);
        return "";
    }
    
    // B
    if ((instr & 0xFC000000) == 0x14000000) {
        int32_t imm26 = instr & 0x3FFFFFF;
        if (imm26 & 0x2000000) imm26 |= 0xFC000000;
        uart::printf("b #%d", imm26 << 2);
        return "";
    }
    
    // BL
    if ((instr & 0xFC000000) == 0x94000000) {
        int32_t imm26 = instr & 0x3FFFFFF;
        if (imm26 & 0x2000000) imm26 |= 0xFC000000;
        uart::printf("bl #%d", imm26 << 2);
        return "";
    }
    
    // B.cond (conditional branch)
    if ((instr & 0xFF000010) == 0x54000000) {
        int cond = instr & 0xF;
        int32_t imm19 = (instr >> 5) & 0x7FFFF;
        if (imm19 & 0x40000) imm19 |= 0xFFF80000;
        
        const char* cond_names[] = {
            "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
            "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
        };
        uart::printf("b.%s #%d", cond_names[cond], imm19 << 2);
        return "";
    }
    
    // SUBS register 64-bit (CMP)
    if ((instr & 0xFF200000) == 0xEB000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        if (rd == 31) {
            uart::printf("cmp x%d, x%d", rn, rm);
        } else {
            uart::printf("subs x%d, x%d, x%d", rd, rn, rm);
        }
        return "";
    }
    
    // SUBS immediate 64-bit (CMP)
    if ((instr & 0xFF000000) == 0xF1000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (rd == 31) {
            uart::printf("cmp x%d, #%d", rn, imm12);
        } else {
            uart::printf("subs x%d, x%d, #%d", rd, rn, imm12);
        }
        return "";
    }
    
    // SUB register 64-bit
    if ((instr & 0xFF200000) == 0xCB000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("sub x%d, x%d, x%d", rd, rn, rm);
        return "";
    }
    
    // SUB immediate 64-bit
    if ((instr & 0xFF000000) == 0xD1000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        uart::printf("sub x%d, x%d, #%d", rd, rn, imm12);
        return "";
    }
    
    // ORR register 64-bit (MOV alias when Rn=XZR)
    if ((instr & 0xFF200000) == 0xAA000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        if (rn == 31) {
            uart::printf("mov x%d, x%d", rd, rm);
        } else {
            uart::printf("orr x%d, x%d, x%d", rd, rn, rm);
        }
        return "";
    }
    
    // STRB (unsigned offset): 00 111 0 01 00 imm12 Rn Rt
    if ((instr & 0xFFC00000) == 0x39000000) {
        int rt = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (imm12 == 0) {
            uart::printf("strb w%d, [x%d]", rt, rn);
        } else {
            uart::printf("strb w%d, [x%d, #%d]", rt, rn, imm12);
        }
        return "";
    }
    
    // LDRB (unsigned offset): 00 111 0 01 01 imm12 Rn Rt
    if ((instr & 0xFFC00000) == 0x39400000) {
        int rt = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (imm12 == 0) {
            uart::printf("ldrb w%d, [x%d]", rt, rn);
        } else {
            uart::printf("ldrb w%d, [x%d, #%d]", rt, rn, imm12);
        }
        return "";
    }
    
    // STR 64-bit (unsigned offset): 11 111 0 01 00 imm12 Rn Rt
    if ((instr & 0xFFC00000) == 0xF9000000) {
        int rt = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (imm12 == 0) {
            uart::printf("str x%d, [x%d]", rt, rn);
        } else {
            uart::printf("str x%d, [x%d, #%d]", rt, rn, imm12 << 3);
        }
        return "";
    }
    
    // LDR 64-bit (unsigned offset): 11 111 0 01 01 imm12 Rn Rt
    if ((instr & 0xFFC00000) == 0xF9400000) {
        int rt = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (imm12 == 0) {
            uart::printf("ldr x%d, [x%d]", rt, rn);
        } else {
            uart::printf("ldr x%d, [x%d, #%d]", rt, rn, imm12 << 3);
        }
        return "";
    }
    
    // MUL 64-bit
    if ((instr & 0xFFE0FC00) == 0x9B007C00) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("mul x%d, x%d, x%d", rd, rn, rm);
        return "";
    }
    
    // MUL 32-bit
    if ((instr & 0xFFE0FC00) == 0x1B007C00) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("mul w%d, w%d, w%d", rd, rn, rm);
        return "";
    }
    
    // STR 32-bit (unsigned offset): 10 111 0 01 00 imm12 Rn Rt
    if ((instr & 0xFFC00000) == 0xB9000000) {
        int rt = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (imm12 == 0) {
            uart::printf("str w%d, [x%d]", rt, rn);
        } else {
            uart::printf("str w%d, [x%d, #%d]", rt, rn, imm12 << 2);
        }
        return "";
    }
    
    // LDR 32-bit (unsigned offset): 10 111 0 01 01 imm12 Rn Rt
    if ((instr & 0xFFC00000) == 0xB9400000) {
        int rt = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (imm12 == 0) {
            uart::printf("ldr w%d, [x%d]", rt, rn);
        } else {
            uart::printf("ldr w%d, [x%d, #%d]", rt, rn, imm12 << 2);
        }
        return "";
    }
    
    // ADD immediate 32-bit
    if ((instr & 0xFF000000) == 0x11000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        uart::printf("add w%d, w%d, #%d", rd, rn, imm12);
        return "";
    }
    
    // SUB immediate 32-bit
    if ((instr & 0xFF000000) == 0x51000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        uart::printf("sub w%d, w%d, #%d", rd, rn, imm12);
        return "";
    }
    
    // SUBS immediate 32-bit (CMP)
    if ((instr & 0xFF000000) == 0x71000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        uint32_t imm12 = (instr >> 10) & 0xFFF;
        if (rd == 31) {
            uart::printf("cmp w%d, #%d", rn, imm12);
        } else {
            uart::printf("subs w%d, w%d, #%d", rd, rn, imm12);
        }
        return "";
    }
    
    // SUBS register 32-bit (CMP)
    if ((instr & 0xFF200000) == 0x6B000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        if (rd == 31) {
            uart::printf("cmp w%d, w%d", rn, rm);
        } else {
            uart::printf("subs w%d, w%d, w%d", rd, rn, rm);
        }
        return "";
    }
    
    // ADD register 32-bit
    if ((instr & 0xFF200000) == 0x0B000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("add w%d, w%d, w%d", rd, rn, rm);
        return "";
    }
    
    // SUB register 32-bit
    if ((instr & 0xFF200000) == 0x4B000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("sub w%d, w%d, w%d", rd, rn, rm);
        return "";
    }
    
    // SUB register 64-bit
    if ((instr & 0xFF200000) == 0xCB000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        uart::printf("sub x%d, x%d, x%d", rd, rn, rm);
        return "";
    }
    
    // ORR register 32-bit (MOV alias)
    if ((instr & 0xFF200000) == 0x2A000000) {
        int rd = instr & 0x1F;
        int rn = (instr >> 5) & 0x1F;
        int rm = (instr >> 16) & 0x1F;
        if (rn == 31) {
            uart::printf("mov w%d, w%d", rd, rm);
        } else {
            uart::printf("orr w%d, w%d, w%d", rd, rn, rm);
        }
        return "";
    }
    
    // MOVN 64-bit
    if ((instr & 0xFF800000) == 0x92800000) {
        int rd = instr & 0x1F;
        uint16_t imm16 = (instr >> 5) & 0xFFFF;
        int hw = (instr >> 21) & 0x3;
        int64_t value = ~((uint64_t)imm16 << (hw * 16));
        uart::printf("mov x%d, #%d", rd, (int)value);
        return "";
    }
    
    return "???";
}

/*
 * casm disasm - Disassemble a binary file
 */
static void cmd_casm_disasm(const char* filename) {
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        uart::printf("casm disasm: %s: No such file\n", filename);
        return;
    }
    
    static uint8_t buffer[4096];
    size_t size = ramfs::read_file(file, buffer, 0, sizeof(buffer));
    
    uart::printf("Disassembly of '%s' (%d bytes):\n\n", filename, (int)size);
    
    char instr_buf[64];
    for (size_t i = 0; i + 3 < size; i += 4) {
        uint32_t instr = buffer[i] | (buffer[i+1] << 8) | 
                        (buffer[i+2] << 16) | (buffer[i+3] << 24);
        
        uart::printf("%04x:  %08x  ", (uint32_t)i, instr);
        const char* dis = disasm_instruction(instr, instr_buf, sizeof(instr_buf));
        if (dis && dis[0]) {
            uart::puts(dis);
        }
        uart::putc('\n');
    }
}

/*
 * casm run with debug mode
 */
static void cmd_casm_run_debug(const char* filename) {
    ramfs::FSNode* file = ramfs::open_file(filename);
    if (!file) {
        uart::printf("casm run: %s: No such file\n", filename);
        return;
    }
    
    static uint8_t code_buffer[5120];
    for (size_t i = 0; i < sizeof(code_buffer); i++) code_buffer[i] = 0;
    size_t actual_code_size = ramfs::read_file(file, code_buffer, 0, file->size);
    size_t mem_size = sizeof(code_buffer);
    
    uint64_t regs[32];
    for (int i = 0; i < 32; i++) regs[i] = 0;
    
    uint64_t pc = 0;
    bool running = true;
    
    uart::puts("Debug mode: Press ENTER to step, 'r' to run, 'q' to quit\n\n");
    
    while (running && pc < actual_code_size) {
        uint32_t instr = code_buffer[pc] | (code_buffer[pc+1] << 8) |
                        (code_buffer[pc+2] << 16) | (code_buffer[pc+3] << 24);
        
        // Show current instruction
        uart::printf("PC=%04x: %08x  ", (uint32_t)pc, instr);
        char buf[64];
        const char* dis = disasm_instruction(instr, buf, sizeof(buf));
        if (dis && dis[0]) uart::puts(dis);
        uart::putc('\n');
        
        // Show registers
        uart::puts("  x0-x3: ");
        for (int i = 0; i < 4; i++) {
            uart::printf("%08x ", (uint32_t)regs[i]);
        }
        uart::putc('\n');
        
        // Wait for input
        char c = uart::getc();
        if (c == 'q' || c == 'Q') {
            uart::puts("Quit\n");
            return;
        }
        
        // Execute instruction (simplified - reuse VM logic)
        // For brevity, just step through
        if ((instr & 0xFFE0001F) == 0xD4000001) {
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            if (imm16 == 0x1FF) running = false;
            else if (imm16 == 0x101) uart::putc(regs[0] & 0xFF);
            else if (imm16 == 0x102) uart::printf("%d", (int)regs[0]);
        }
        else if ((instr & 0xFF800000) == 0x52800000) {
            int rd = instr & 0x1F;
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            if (rd != 31) regs[rd] = imm16;
        }
        else if ((instr & 0xFF800000) == 0xD2800000) {
            int rd = instr & 0x1F;
            uint16_t imm16 = (instr >> 5) & 0xFFFF;
            int hw = (instr >> 21) & 0x3;
            if (rd != 31) regs[rd] = static_cast<uint64_t>(imm16) << (hw * 16);
        }
        else if ((instr & 0xFF200000) == 0x8B000000) {
            int rd = instr & 0x1F;
            int rn = (instr >> 5) & 0x1F;
            int rm = (instr >> 16) & 0x1F;
            if (rd != 31) regs[rd] = regs[rn] + regs[rm];
        }
        else if ((instr & 0xFFFFFC1F) == 0xD65F0000) {
            running = false;
        }
        
        pc += 4;
    }
    
    uart::puts("\nExecution complete\n");
}

// ============================================================================
// Process Commands: ps and top
// ============================================================================

/*
 * ps - List processes
 */
void cmd_ps(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    uart::puts("  PID  STATE     CPU(ms)  MEM(KB)  NAME\n");
    uart::puts("-----  --------  -------  -------  ----------------\n");
    
    // Always show kernel and shell as "processes"
    uint64_t uptime = timer::get_uptime_ms();
    uart::printf("%5d  running   %7d  %7d  %s\n", 0, (int)uptime, 128, "[kernel]");
    uart::printf("%5d  running   %7d  %7d  %s\n", 1, (int)(uptime/2), 64, "[shell]");
    
    // Show any tracked processes
    int count = shell::get_process_count();
    for (int i = 0; i < count; i++) {
        shell::ProcessInfo* proc = shell::get_process_by_index(i);
        if (proc) {
            const char* state_str = "unknown ";
            switch (proc->state) {
                case shell::ProcessState::RUNNING: state_str = "running "; break;
                case shell::ProcessState::SLEEPING: state_str = "sleeping"; break;
                case shell::ProcessState::STOPPED: state_str = "stopped "; break;
                default: break;
            }
            uart::printf("%5d  %s  %7d  %7d  %s\n", 
                proc->pid, state_str, (int)proc->cpu_time, 
                (int)(proc->memory / 1024), proc->name);
        }
    }
}

/*
 * top - Interactive process viewer
 * Arrow keys to navigate, 'k' to kill, 'q' to quit
 */
void cmd_top(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    int selected = 0;
    bool running = true;
    
    while (running) {
        // Clear screen and draw header
        uart::puts("\x1b[2J\x1b[H");
        uart::puts("\x1b[7m EmberOS Process Viewer - Press 'q' to quit, 'k' to kill \x1b[0m\n\n");
        
        // System stats
        uint64_t uptime = timer::get_uptime_ms();
        memory::MemStats mem = memory::get_stats();
        uint32_t mem_used_kb = (mem.used_pages * memory::PAGE_SIZE) / 1024;
        uint32_t mem_total_kb = (mem.total_pages * memory::PAGE_SIZE) / 1024;
        
        uart::printf("Uptime: %d.%03ds  Memory: %dKB / %dKB (%d%%)\n\n",
            (int)(uptime / 1000), (int)(uptime % 1000),
            mem_used_kb, mem_total_kb,
            mem.total_pages ? (int)((mem.used_pages * 100) / mem.total_pages) : 0);
        
        // Column headers
        uart::puts("  PID  STATE     CPU(ms)  MEM(KB)  NAME\n");
        uart::puts("-----  --------  -------  -------  ----------------\n");
        
        // Build process list (kernel + shell + tracked)
        struct ProcEntry {
            int pid;
            char state[12];
            uint32_t cpu;
            uint32_t mem;
            char name[32];
        };
        
        ProcEntry entries[20];
        int entry_count = 0;
        
        // Helper to copy strings
        auto str_cpy = [](char* dst, const char* src, int max) {
            int i = 0;
            while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
            dst[i] = '\0';
        };
        
        // Kernel
        entries[entry_count].pid = 0;
        str_cpy(entries[entry_count].state, "running ", 12);
        entries[entry_count].cpu = (uint32_t)uptime;
        entries[entry_count].mem = 128;
        str_cpy(entries[entry_count].name, "[kernel]", 32);
        entry_count++;
        
        // Shell
        entries[entry_count].pid = 1;
        str_cpy(entries[entry_count].state, "running ", 12);
        entries[entry_count].cpu = (uint32_t)(uptime/2);
        entries[entry_count].mem = 64;
        str_cpy(entries[entry_count].name, "[shell]", 32);
        entry_count++;
        
        // Tracked processes
        int proc_count = shell::get_process_count();
        for (int i = 0; i < proc_count && entry_count < 20; i++) {
            shell::ProcessInfo* proc = shell::get_process_by_index(i);
            if (proc) {
                entries[entry_count].pid = proc->pid;
                switch (proc->state) {
                    case shell::ProcessState::RUNNING: 
                        str_cpy(entries[entry_count].state, "running ", 12); break;
                    case shell::ProcessState::SLEEPING: 
                        str_cpy(entries[entry_count].state, "sleeping", 12); break;
                    case shell::ProcessState::STOPPED: 
                        str_cpy(entries[entry_count].state, "stopped ", 12); break;
                    default: 
                        str_cpy(entries[entry_count].state, "unknown ", 12); break;
                }
                entries[entry_count].cpu = (uint32_t)proc->cpu_time;
                entries[entry_count].mem = proc->memory / 1024;
                str_cpy(entries[entry_count].name, proc->name, 32);
                entry_count++;
            }
        }
        
        // Clamp selection
        if (selected >= entry_count) selected = entry_count - 1;
        if (selected < 0) selected = 0;
        
        // Display entries
        for (int i = 0; i < entry_count; i++) {
            if (i == selected) {
                uart::puts("\x1b[7m");  // Inverse video for selection
            }
            uart::printf("%5d  %s  %7d  %7d  %s", 
                entries[i].pid, entries[i].state, 
                entries[i].cpu, entries[i].mem, entries[i].name);
            if (i == selected) {
                uart::puts("\x1b[0m");
            }
            uart::putc('\n');
        }
        
        uart::puts("\n[Up/Down] Navigate  [k] Kill  [q] Quit\n");
        
        // Wait for input (with timeout for refresh)
        uint64_t start = timer::get_uptime_ms();
        bool got_input = false;
        
        while (timer::get_uptime_ms() - start < 1000 && !got_input) {
            if (uart::has_input()) {
                got_input = true;
                char c = uart::getc();
                
                if (c == 'q' || c == 'Q' || c == '\x03') {  // q or Ctrl+C
                    running = false;
                } else if (c == 'k' || c == 'K') {
                    // Kill selected process
                    if (selected >= 2 && entries[selected].pid > 1) {
                        shell::destroy_process(entries[selected].pid);
                        uart::printf("\nKilled process %d\n", entries[selected].pid);
                        timer::sleep_ms(500);
                    } else {
                        uart::puts("\nCannot kill system processes\n");
                        timer::sleep_ms(500);
                    }
                } else if (c == '\x1b') {  // Escape sequence
                    char c2 = uart::getc();
                    if (c2 == '[') {
                        char c3 = uart::getc();
                        if (c3 == 'A') {  // Up
                            if (selected > 0) selected--;
                        } else if (c3 == 'B') {  // Down
                            if (selected < entry_count - 1) selected++;
                        }
                    }
                }
            }
            timer::sleep_ms(10);
        }
    }
    
    // Clear screen on exit
    uart::puts("\x1b[2J\x1b[H");
}

// ============================================================================
// Command Registration
// ============================================================================

/*
 * Register all built-in commands with the shell
 */
void register_all() {
    // Basic commands (Requirements: 6.1, 6.2, 6.3, 6.4)
    shell::register_command("help", "Display available commands", cmd_help);
    shell::register_command("clear", "Clear the screen", cmd_clear);
    shell::register_command("echo", "Print arguments to console", cmd_echo);
    shell::register_command("version", "Display EmberOS version", cmd_version);
    
    // System info commands (Requirements: 6.5, 6.6, 6.7, 6.10)
    shell::register_command("uptime", "Display system uptime", cmd_uptime);
    shell::register_command("meminfo", "Display memory statistics", cmd_meminfo);
    shell::register_command("cpuinfo", "Display CPU information", cmd_cpuinfo);
    shell::register_command("date", "Display system time", cmd_date);
    
    // System control commands (Requirements: 6.8, 6.9)
    shell::register_command("reboot", "Restart the system", cmd_reboot);
    shell::register_command("shutdown", "Halt the system", cmd_shutdown);
    
    // Utility commands (Requirements: 6.13)
    shell::register_command("hexdump", "Display memory in hex", cmd_hexdump);
    
    // Filesystem commands
    shell::register_command("ls", "List directory contents", cmd_ls);
    shell::register_command("cd", "Change directory", cmd_cd);
    shell::register_command("pwd", "Print working directory", cmd_pwd);
    shell::register_command("mkdir", "Create directory", cmd_mkdir);
    shell::register_command("rmdir", "Remove empty directory", cmd_rmdir);
    shell::register_command("rm", "Remove file or directory", cmd_rm);
    shell::register_command("touch", "Create empty file", cmd_touch);
    shell::register_command("cat", "Display file contents", cmd_cat);
    shell::register_command("vi", "Text editor", cmd_vi);
    shell::register_command("write", "Write text to file (Ctrl+D to save)", cmd_write);
    shell::register_command("cp", "Copy file", cmd_cp);
    shell::register_command("mv", "Move/rename file", cmd_mv);
    shell::register_command("head", "Display first lines of file", cmd_head);
    shell::register_command("tail", "Display last lines of file", cmd_tail);
    shell::register_command("wc", "Word/line/char count", cmd_wc);
    shell::register_command("grep", "Search for pattern in file", cmd_grep);
    shell::register_command("find", "Search for files", cmd_find);
    shell::register_command("df", "Display filesystem usage", cmd_df);
    shell::register_command("xxd", "Hex dump of file", cmd_xxd);
    
    // Shell commands
    shell::register_command("history", "Display command history", cmd_history);
    shell::register_command("alias", "Manage command aliases", cmd_alias);
    shell::register_command("unalias", "Remove an alias", cmd_unalias);
    shell::register_command("env", "Display environment variables", cmd_env);
    shell::register_command("export", "Set environment variable", cmd_export);
    shell::register_command("unset", "Remove environment variable", cmd_unset);
    shell::register_command("time", "Time command execution", cmd_time);
    shell::register_command("whoami", "Display current user", cmd_whoami);
    shell::register_command("hostname", "Display/set hostname", cmd_hostname);
    
    // Process commands
    shell::register_command("ps", "List processes", cmd_ps);
    shell::register_command("top", "Interactive process viewer", cmd_top);
    
    // Developer commands
    shell::register_command("regs", "Display CPU registers", cmd_regs);
    
    // CASM assembler (Requirements: 7.11)
    shell::register_command("casm", "Assemble/run/disasm C.ASM files", cmd_casm);
}

} // namespace commands
