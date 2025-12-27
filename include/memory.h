/*
 * EmberOS Memory Manager Header
 * Physical page allocator using bitmap-based tracking
 * 
 * Requirements: 2.1, 2.2, 2.3, 2.4
 */

#ifndef EMBEROS_MEMORY_H
#define EMBEROS_MEMORY_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace memory {

// Page size constant (4KB)
constexpr size_t PAGE_SIZE = 4096;

// Memory statistics structure
struct MemStats {
    size_t total_pages;
    size_t free_pages;
    size_t used_pages;
};

/*
 * Initialize memory manager with available RAM range
 * Detects available physical memory and sets up page tracking
 * Requirements: 2.1
 */
void init(uintptr_t start, uintptr_t end);

/*
 * Allocate n contiguous pages
 * Returns physical address of allocated pages, or nullptr on failure
 * Requirements: 2.2, 2.3
 */
void* alloc_pages(size_t n);

/*
 * Free previously allocated pages
 * Marks pages as available for reuse
 * Requirements: 2.4
 */
void free_pages(void* addr, size_t n);

/*
 * Get memory statistics
 * Returns total, free, and used page counts
 */
MemStats get_stats();

} // namespace memory

#endif // EMBEROS_MEMORY_H
