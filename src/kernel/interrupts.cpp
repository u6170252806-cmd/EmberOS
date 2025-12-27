/*
 * EmberOS Interrupt Handler Implementation
 * ARM64 exception handling
 * 
 * Requirements: 3.2, 3.3, 3.4
 */

#include "interrupts.h"
#include "uart.h"
#include "timer.h"
#include "ramfs.h"

// Forward declaration of GIC functions (implemented in gic.cpp)
namespace gic {
    void init();
    void enable_irq(uint32_t irq);
    void disable_irq(uint32_t irq);
    uint32_t acknowledge_irq();
    void end_irq(uint32_t irq);
}

// CASM native execution state
namespace casm_native {
    static uint8_t* code_buffer = nullptr;
    static size_t mem_size = 0;
    static uintptr_t base_addr = 0;  // Base address of code_buffer
    static bool running = false;
    static bool halt_requested = false;
    
    // Performance counters (for profiling)
    static uint64_t svc_count = 0;
    static uint64_t start_tick = 0;
    
    // Output buffer for batched UART (reduces MMIO overhead)
    static char out_buffer[256];
    static int out_pos = 0;
    
    // Flush output buffer to UART
    void flush_output() {
        for (int i = 0; i < out_pos; i++) {
            uart::putc(out_buffer[i]);
        }
        out_pos = 0;
    }
    
    // Buffered character output - flushes on newline or when full
    inline void buffered_putc(char c) {
        out_buffer[out_pos++] = c;
        if (c == '\n' || out_pos >= 255) {
            flush_output();
        }
    }
    
    // Buffered string output
    inline void buffered_puts(const char* s) {
        while (*s) {
            buffered_putc(*s++);
        }
    }
    
    // Buffered number output (decimal)
    void buffered_print_num(int64_t value) {
        if (value < 0) {
            buffered_putc('-');
            value = -value;
        }
        if (value == 0) {
            buffered_putc('0');
            return;
        }
        char buf[20];
        int i = 0;
        while (value > 0) {
            buf[i++] = '0' + (value % 10);
            value /= 10;
        }
        while (i > 0) buffered_putc(buf[--i]);
    }
    
    // Buffered hex output
    void buffered_print_hex(uint32_t value) {
        const char* hex = "0123456789abcdef";
        buffered_putc('0');
        buffered_putc('x');
        bool started = false;
        for (int i = 28; i >= 0; i -= 4) {
            int digit = (value >> i) & 0xF;
            if (digit || started || i == 0) {
                buffered_putc(hex[digit]);
                started = true;
            }
        }
    }
    
    // Graphics framebuffer
    static char framebuffer[25][81];
    static char fb_colors[25][80];
    static int fb_width = 0;
    static int fb_height = 0;
    static bool fb_active = false;
    static int fb_fg = 7;
    static int fb_bg = 0;
    
    // Better RNG state (xorshift64)
    static uint64_t rng_state = 0x853c49e6748fea9bULL;
    
    void fb_clear() {
        for (int y = 0; y < 25; y++) {
            for (int x = 0; x < 80; x++) {
                framebuffer[y][x] = ' ';
                fb_colors[y][x] = 0x70;
            }
            framebuffer[y][80] = '\0';
        }
    }
    
    void fb_plot(int x, int y, char ch) {
        if (x >= 0 && x < fb_width && y >= 0 && y < fb_height) {
            framebuffer[y][x] = ch ? ch : ' ';
            fb_colors[y][x] = (fb_fg & 0x7) | ((fb_bg & 0x7) << 4);
        }
    }
    
    void fb_render() {
        if (!fb_active || fb_height == 0) return;
        int last_fg = -1, last_bg = -1;
        for (int y = 0; y < fb_height; y++) {
            for (int x = 0; x < fb_width; x++) {
                int fg = fb_colors[y][x] & 0x7;
                int bg = (fb_colors[y][x] >> 4) & 0x7;
                if (fg != last_fg || bg != last_bg) {
                    uart::printf("\x1b[3%d;4%dm", fg, bg);
                    last_fg = fg;
                    last_bg = bg;
                }
                uart::putc(framebuffer[y][x]);
            }
            uart::puts("\x1b[0m\n");
            last_fg = last_bg = -1;
        }
    }
    
    void init(uint8_t* buffer, size_t size) {
        code_buffer = buffer;
        mem_size = size;
        base_addr = (uintptr_t)buffer;
        running = true;
        halt_requested = false;
        svc_count = 0;
        start_tick = timer::get_uptime_ms();
        out_pos = 0;  // Clear output buffer
        fb_active = false;
        fb_width = 0;
        fb_height = 0;
        fb_fg = 7;
        fb_bg = 0;
        fb_clear();
        // Seed RNG with timer
        rng_state ^= start_tick;
        if (rng_state == 0) rng_state = 0x853c49e6748fea9bULL;
    }
    
    void cleanup() {
        flush_output();  // Flush any remaining buffered output
        if (fb_active) fb_render();
        uart::puts("\x1b[0m");
        running = false;
        code_buffer = nullptr;
        base_addr = 0;
    }
    
    // Fast xorshift64 RNG
    inline uint64_t xorshift64() {
        uint64_t x = rng_state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        rng_state = x;
        return x;
    }
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

// CASM native execution interface
namespace casm_native {
    extern void init(uint8_t* buffer, size_t size);
    extern void cleanup();
    extern bool halt_requested;
    extern bool running;
}

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
    
    // Fast path: Handle SVC (syscall) - EC = 0x15
    if (ec == 0x15 && casm_native::running) {
        uint16_t svc_num = ctx->esr & 0xFFFF;
        uint64_t* regs = ctx->x;
        casm_native::svc_count++;
        
        // Pre-compute for address validation
        const uintptr_t base = casm_native::base_addr;
        const size_t mem_size = casm_native::mem_size;
        
        // Inline address validation macro (faster than lambda)
        #define SAFE_PTR(addr) \
            (((addr) >= base && (addr) < base + mem_size) ? (uint8_t*)(addr) : \
             ((addr) < mem_size) ? (uint8_t*)(base + (addr)) : nullptr)
        
        // Most common opcodes first for branch prediction
        // Using buffered output for better performance
        switch (svc_num) {
            case 0x101: // PRTC - most common, use buffered output
                casm_native::buffered_putc(regs[0] & 0xFF);
                return;
            case 0x102: // PRTN - use buffered number output
                casm_native::buffered_print_num((int64_t)regs[0]);
                return;
            case 0x103: // INP - must flush before reading
                casm_native::flush_output();
                regs[0] = uart::getc();
                return;
            case 0x100: { // PRT - use buffered string output
                uint8_t* ptr = SAFE_PTR(regs[0]);
                if (ptr) while (*ptr) casm_native::buffered_putc(*ptr++);
                return;
            }
            case 0x104: { // INPS - must flush before reading
                casm_native::flush_output();
                uint8_t* ptr = SAFE_PTR(regs[0]);
                uint64_t max = regs[1] ? regs[1] : 64;
                if (max > 256) max = 256;
                uint64_t i = 0;
                if (ptr) {
                    while (i < max - 1) {
                        char c = uart::getc();
                        if (c == '\r' || c == '\n') { uart::putc('\n'); break; }
                        if (c == 127 || c == 8) { if (i > 0) { i--; uart::puts("\b \b"); } continue; }
                        uart::putc(c);
                        ptr[i++] = c;
                    }
                    ptr[i] = 0;
                }
                regs[0] = i;
                return;
            }
            case 0x105: // PRTX - use buffered hex output
                casm_native::buffered_print_hex((uint32_t)regs[0]);
                return;
            
            // Graphics opcodes
            case 0x110: // CLS
                if (!casm_native::fb_active) { casm_native::fb_active = true; casm_native::fb_width = 40; casm_native::fb_height = 12; }
                casm_native::fb_clear();
                return;
            case 0x111: // SETC - also store computed color in x24 for native plot
                if (!casm_native::fb_active) { casm_native::fb_active = true; casm_native::fb_width = 40; casm_native::fb_height = 12; casm_native::fb_clear(); }
                casm_native::fb_fg = regs[0] & 7;
                casm_native::fb_bg = regs[1] & 7;
                // Store computed color in x24 for native plot to use
                regs[24] = (casm_native::fb_fg & 0x7) | ((casm_native::fb_bg & 0x7) << 4);
                return;
            case 0x112: // PLOT - kept for backward compatibility, native plot bypasses this
                if (!casm_native::fb_active) { casm_native::fb_active = true; casm_native::fb_width = 40; casm_native::fb_height = 12; casm_native::fb_clear(); }
                casm_native::fb_plot(regs[0] & 0xFF, regs[1] & 0xFF, regs[2] & 0xFF);
                return;
            case 0x113: { // LINE
                if (!casm_native::fb_active) { casm_native::fb_active = true; casm_native::fb_width = 40; casm_native::fb_height = 12; casm_native::fb_clear(); }
                int x1 = regs[0] & 0xFF, y1 = regs[1] & 0xFF, x2 = regs[2] & 0xFF, y2 = regs[3] & 0xFF;
                char ch = (regs[4] & 0xFF) ? (regs[4] & 0xFF) : '*';
                if (y1 == y2) { for (int i = (x1<x2?x1:x2); i <= (x1>x2?x1:x2); i++) casm_native::fb_plot(i, y1, ch); }
                else if (x1 == x2) { for (int i = (y1<y2?y1:y2); i <= (y1>y2?y1:y2); i++) casm_native::fb_plot(x1, i, ch); }
                return;
            }
            case 0x114: { // BOX
                if (!casm_native::fb_active) { casm_native::fb_active = true; casm_native::fb_width = 40; casm_native::fb_height = 12; casm_native::fb_clear(); }
                int x = regs[0] & 0xFF, y = regs[1] & 0xFF, w = regs[2] & 0xFF, h = regs[3] & 0xFF;
                casm_native::fb_plot(x, y, '+'); for (int i = 1; i < w-1; i++) casm_native::fb_plot(x+i, y, '-'); casm_native::fb_plot(x+w-1, y, '+');
                for (int i = 1; i < h-1; i++) { casm_native::fb_plot(x, y+i, '|'); for (int j = 1; j < w-1; j++) casm_native::fb_plot(x+j, y+i, ' '); casm_native::fb_plot(x+w-1, y+i, '|'); }
                casm_native::fb_plot(x, y+h-1, '+'); for (int i = 1; i < w-1; i++) casm_native::fb_plot(x+i, y+h-1, '-'); casm_native::fb_plot(x+w-1, y+h-1, '+');
                return;
            }
            case 0x115: // RESET - also update x24 with default color
                casm_native::fb_fg = 7; casm_native::fb_bg = 0;
                regs[24] = 0x70;  // white on black
                return;
            case 0x116: { // CANVAS - also set default color in x24
                casm_native::fb_width = regs[0] & 0xFF; casm_native::fb_height = regs[1] & 0xFF;
                if (casm_native::fb_width < 1) casm_native::fb_width = 40; if (casm_native::fb_height < 1) casm_native::fb_height = 10;
                if (casm_native::fb_width > 80) casm_native::fb_width = 80; if (casm_native::fb_height > 24) casm_native::fb_height = 24;
                casm_native::fb_active = true; casm_native::fb_clear();
                // Set default color (white on black) in x24 for native plot
                regs[24] = 0x70;
                return;
            }
            
            // File opcodes
            case 0x120: { // FCREAT
                char fname[64]; uint8_t* ptr = SAFE_PTR(regs[0]); int j = 0;
                if (ptr) while (j < 63 && ptr[j]) { fname[j] = ptr[j]; j++; }
                fname[j] = 0;
                regs[0] = ramfs::create_file(fname) ? 1 : 0;
                return;
            }
            case 0x121: { // FWRITE
                char fname[64]; uint8_t* name_ptr = SAFE_PTR(regs[0]); uint8_t* data_ptr = SAFE_PTR(regs[1]);
                uint64_t len = regs[2]; int j = 0;
                if (name_ptr) while (j < 63 && name_ptr[j]) { fname[j] = name_ptr[j]; j++; }
                fname[j] = 0;
                ramfs::FSNode* f = ramfs::open_file(fname); if (!f) f = ramfs::create_file(fname);
                if (f && data_ptr) { ramfs::write_file(f, data_ptr, 0, len); regs[0] = len; } else regs[0] = 0;
                return;
            }
            case 0x122: { // FREAD
                char fname[64]; uint8_t* name_ptr = SAFE_PTR(regs[0]); uint8_t* buf_ptr = SAFE_PTR(regs[1]);
                uint64_t max_len = regs[2]; int j = 0;
                if (name_ptr) while (j < 63 && name_ptr[j]) { fname[j] = name_ptr[j]; j++; }
                fname[j] = 0;
                ramfs::FSNode* f = ramfs::open_file(fname);
                regs[0] = (f && buf_ptr) ? ramfs::read_file(f, buf_ptr, 0, max_len) : 0;
                return;
            }
            case 0x123: { // FDEL
                char fname[64]; uint8_t* ptr = SAFE_PTR(regs[0]); int j = 0;
                if (ptr) while (j < 63 && ptr[j]) { fname[j] = ptr[j]; j++; }
                fname[j] = 0;
                regs[0] = ramfs::delete_file(fname) ? 1 : 0;
                return;
            }
            case 0x124: case 0x125: { // FCOPY / FMOVE
                char src[64], dst[64]; uint8_t* src_ptr = SAFE_PTR(regs[0]); uint8_t* dst_ptr = SAFE_PTR(regs[1]); int j = 0;
                if (src_ptr) while (j < 63 && src_ptr[j]) { src[j] = src_ptr[j]; j++; }
                src[j] = 0; j = 0;
                if (dst_ptr) while (j < 63 && dst_ptr[j]) { dst[j] = dst_ptr[j]; j++; }
                dst[j] = 0;
                ramfs::FSNode* sf = ramfs::open_file(src);
                if (sf) {
                    static uint8_t cb[1024]; size_t sz = ramfs::read_file(sf, cb, 0, sizeof(cb));
                    ramfs::FSNode* df = ramfs::create_file(dst);
                    if (df) { ramfs::write_file(df, cb, 0, sz); if (svc_num == 0x125) ramfs::delete_file(src); regs[0] = 1; }
                    else regs[0] = 0;
                } else regs[0] = 0;
                return;
            }
            case 0x126: { // FEXIST
                char fname[64]; uint8_t* ptr = SAFE_PTR(regs[0]); int j = 0;
                if (ptr) while (j < 63 && ptr[j]) { fname[j] = ptr[j]; j++; }
                fname[j] = 0;
                regs[0] = ramfs::open_file(fname) ? 1 : 0;
                return;
            }
            
            // Memory/String opcodes (optimized with word-sized ops)
            case 0x130: { // STRLEN
                uint8_t* ptr = SAFE_PTR(regs[0]); uint64_t len = 0;
                if (ptr) while (ptr[len]) len++;
                regs[0] = len;
                return;
            }
            case 0x131: { // MEMCPY - optimized with 64-bit copies
                uint8_t* dst = SAFE_PTR(regs[0]); uint8_t* src = SAFE_PTR(regs[1]); uint64_t len = regs[2];
                if (dst && src) {
                    // Word-aligned fast path
                    while (len >= 8 && !((uintptr_t)dst & 7) && !((uintptr_t)src & 7)) {
                        *(uint64_t*)dst = *(uint64_t*)src; dst += 8; src += 8; len -= 8;
                    }
                    while (len--) *dst++ = *src++;
                }
                return;
            }
            case 0x132: { // MEMSET - optimized with 64-bit fills
                uint8_t* ptr = SAFE_PTR(regs[0]); uint8_t val = regs[1] & 0xFF; uint64_t len = regs[2];
                if (ptr) {
                    uint64_t val64 = val * 0x0101010101010101ULL;
                    while (len >= 8 && !((uintptr_t)ptr & 7)) { *(uint64_t*)ptr = val64; ptr += 8; len -= 8; }
                    while (len--) *ptr++ = val;
                }
                return;
            }
            case 0x133: { // ABS
                int64_t v = (int64_t)regs[0]; regs[0] = (v < 0) ? -v : v;
                return;
            }
            
            // System opcodes
            case 0x1F0: // SLEEP - flush output first so text appears before delay
                casm_native::flush_output();
                timer::sleep_ms(regs[0] & 0xFFFF);
                return;
            case 0x1F1: { // RND - xorshift64 (better quality)
                uint64_t max = regs[0] ? regs[0] : 1;
                regs[0] = casm_native::xorshift64() % max;
                return;
            }
            case 0x1F2: regs[0] = timer::get_uptime_ms(); return;
            case 0x1FF: // HALT
                casm_native::halt_requested = true;
                casm_native::cleanup();
                return;
            
            default:
                uart::printf("\n[casm] Unknown SVC #0x%x\n", svc_num);
                casm_native::halt_requested = true;
                casm_native::cleanup();
                return;
        }
        #undef SAFE_PTR
    }
    
    // Not a CASM SVC - handle as before
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

// CASM native execution - run function
namespace casm_native {
    bool is_halted() {
        return halt_requested;
    }
    
    void run(void* code_start) {
        if (!code_buffer || !running) return;
        
        // Jump to the code - it will execute until it hits an SVC
        // The SVC handler will process opcodes and return
        // When halt (SVC #0x1FF) is called, halt_requested becomes true
        
        // Register allocation for native CASM execution:
        // x28 = code_buffer base address (for data area access)
        // x27 = framebuffer base pointer (for direct plot access)
        // x26 = framebuffer row stride (81 bytes per row)
        // x25 = fb_colors base pointer (for direct color access)
        //
        // This allows plot to write directly without SVC:
        //   fb_addr = x27 + (y * 81) + x
        //   color_addr = x25 + (y * 80) + x
        //   strb char, [fb_addr]
        //   strb color, [color_addr]  (color from fb_fg/fb_bg)
        
        // Call the native code with reserved registers set up
        register uint64_t x28_val asm("x28") = (uint64_t)code_buffer;
        register uint64_t x27_val asm("x27") = (uint64_t)&framebuffer[0][0];
        register uint64_t x26_val asm("x26") = 81;  // Row stride for framebuffer
        register uint64_t x25_val asm("x25") = (uint64_t)&fb_colors[0][0];
        asm volatile(
            "blr %[func]"
            : "+r"(x28_val), "+r"(x27_val), "+r"(x26_val), "+r"(x25_val)
            : [func] "r"(code_start)
            : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
              "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
              "x16", "x17", "x19", "x20", "x21", "x22", "x23", "x24",
              "x29", "x30", "memory"
        );
        
        // If we get here via RET (not halt), cleanup
        // halt already called cleanup in the SVC handler
        if (!halt_requested) {
            cleanup();
        }
    }
}
