/*
 * EmberOS Memory Manager Implementation
 * Physical page allocator using bitmap-based tracking
 * 
 * Requirements: 2.1, 2.2, 2.3, 2.4
 */

#include "memory.h"
#include "uart.h"

namespace memory {

// Maximum supported RAM: 128MB = 32768 pages
// Bitmap: 32768 bits = 512 uint64_t entries
constexpr size_t MAX_PAGES = 32768;
constexpr size_t BITMAP_SIZE = MAX_PAGES / 64;

// Page allocation bitmap (1 = used, 0 = free)
static uint64_t page_bitmap[BITMAP_SIZE];

// Memory range tracking
static uintptr_t mem_start = 0;
static uintptr_t mem_end = 0;
static size_t total_pages = 0;
static size_t used_pages = 0;

// Helper: Set a bit in the bitmap (mark page as used)
static inline void bitmap_set(size_t page_index) {
    if (page_index < MAX_PAGES) {
        page_bitmap[page_index / 64] |= (1ULL << (page_index % 64));
    }
}

// Helper: Clear a bit in the bitmap (mark page as free)
static inline void bitmap_clear(size_t page_index) {
    if (page_index < MAX_PAGES) {
        page_bitmap[page_index / 64] &= ~(1ULL << (page_index % 64));
    }
}

// Helper: Test if a bit is set (page is used)
static inline bool bitmap_test(size_t page_index) {
    if (page_index >= MAX_PAGES) {
        return true;  // Out of range pages are considered used
    }
    return (page_bitmap[page_index / 64] & (1ULL << (page_index % 64))) != 0;
}

// Helper: Convert physical address to page index
static inline size_t addr_to_page(uintptr_t addr) {
    return (addr - mem_start) / PAGE_SIZE;
}

// Helper: Convert page index to physical address
static inline uintptr_t page_to_addr(size_t page_index) {
    return mem_start + (page_index * PAGE_SIZE);
}

/*
 * Initialize memory manager with available RAM range
 * Requirements: 2.1
 */
void init(uintptr_t start, uintptr_t end) {
    // Align start up to page boundary
    mem_start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    // Align end down to page boundary
    mem_end = end & ~(PAGE_SIZE - 1);
    
    // Calculate total pages
    if (mem_end > mem_start) {
        total_pages = (mem_end - mem_start) / PAGE_SIZE;
    } else {
        total_pages = 0;
    }
    
    // Cap at maximum supported pages
    if (total_pages > MAX_PAGES) {
        total_pages = MAX_PAGES;
        mem_end = mem_start + (total_pages * PAGE_SIZE);
    }
    
    // Initialize bitmap: all pages free (0)
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        page_bitmap[i] = 0;
    }
    
    // Mark pages beyond our range as used
    for (size_t i = total_pages; i < MAX_PAGES; i++) {
        bitmap_set(i);
    }
    
    used_pages = 0;
    
    uart::printf("Memory: Initialized %d pages (%d KB) from 0x%x to 0x%x\n",
                 static_cast<unsigned int>(total_pages),
                 static_cast<unsigned int>((total_pages * PAGE_SIZE) / 1024),
                 static_cast<unsigned int>(mem_start),
                 static_cast<unsigned int>(mem_end));
}

/*
 * Allocate n contiguous pages
 * Returns physical address of allocated pages, or nullptr on failure
 * Requirements: 2.2, 2.3
 */
void* alloc_pages(size_t n) {
    if (n == 0 || n > total_pages) {
        return nullptr;
    }
    
    // Search for n contiguous free pages
    size_t consecutive = 0;
    size_t start_page = 0;
    
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            // Page is free
            if (consecutive == 0) {
                start_page = i;
            }
            consecutive++;
            
            if (consecutive == n) {
                // Found enough contiguous pages, mark them as used
                for (size_t j = start_page; j < start_page + n; j++) {
                    bitmap_set(j);
                }
                used_pages += n;
                
                return reinterpret_cast<void*>(page_to_addr(start_page));
            }
        } else {
            // Page is used, reset counter
            consecutive = 0;
        }
    }
    
    // Not enough contiguous pages found
    // Requirements: 2.6 - return null pointer on failure
    return nullptr;
}

/*
 * Free previously allocated pages
 * Marks pages as available for reuse
 * Requirements: 2.4
 */
void free_pages(void* addr, size_t n) {
    if (addr == nullptr || n == 0) {
        return;
    }
    
    uintptr_t phys_addr = reinterpret_cast<uintptr_t>(addr);
    
    // Validate address is within our managed range
    if (phys_addr < mem_start || phys_addr >= mem_end) {
        return;
    }
    
    // Validate address is page-aligned
    if (phys_addr % PAGE_SIZE != 0) {
        return;
    }
    
    size_t start_page = addr_to_page(phys_addr);
    
    // Validate we won't go beyond our range
    if (start_page + n > total_pages) {
        return;
    }
    
    // Mark pages as free
    for (size_t i = start_page; i < start_page + n; i++) {
        if (bitmap_test(i)) {
            bitmap_clear(i);
            if (used_pages > 0) {
                used_pages--;
            }
        }
    }
}

/*
 * Get memory statistics
 * Returns total, free, and used page counts
 */
MemStats get_stats() {
    MemStats stats;
    stats.total_pages = total_pages;
    stats.used_pages = used_pages;
    stats.free_pages = total_pages - used_pages;
    return stats;
}

} // namespace memory
