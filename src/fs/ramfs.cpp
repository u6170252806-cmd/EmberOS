/*
 * EmberOS RAM Filesystem Implementation
 * In-memory filesystem for storing files and directories
 */

#include "ramfs.h"
#include "uart.h"
#include "memory.h"

namespace ramfs {

// ============================================================================
// String Utilities
// ============================================================================

static size_t str_len(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static int str_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

static void str_copy(char* dst, const char* src, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void mem_set(void* ptr, int value, size_t size) {
    uint8_t* p = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++) {
        p[i] = static_cast<uint8_t>(value);
    }
}

static void mem_copy(void* dst, const void* src, size_t size) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    for (size_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

// ============================================================================
// Global State
// ============================================================================

static FSNode g_nodes[MAX_FILES];
static FSNode* g_root = nullptr;
static FSNode* g_cwd = nullptr;
static char g_cwd_path[MAX_PATH];

// ============================================================================
// Internal Helpers
// ============================================================================

static FSNode* alloc_node() {
    for (size_t i = 0; i < MAX_FILES; i++) {
        if (!g_nodes[i].in_use) {
            mem_set(&g_nodes[i], 0, sizeof(FSNode));
            g_nodes[i].in_use = true;
            return &g_nodes[i];
        }
    }
    return nullptr;
}

static void free_node(FSNode* node) {
    if (!node) return;
    
    // Free file data if any
    if (node->data) {
        memory::free_pages(node->data, (node->size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE);
        node->data = nullptr;
    }
    
    node->in_use = false;
}

// Remove node from parent's children list
static void unlink_node(FSNode* node) {
    if (!node || !node->parent) return;
    
    FSNode* parent = node->parent;
    
    if (parent->children == node) {
        parent->children = node->next;
    } else {
        FSNode* prev = parent->children;
        while (prev && prev->next != node) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = node->next;
        }
    }
    
    node->parent = nullptr;
    node->next = nullptr;
}

// Add node to parent's children list
static void link_node(FSNode* parent, FSNode* node) {
    if (!parent || !node) return;
    
    node->parent = parent;
    node->next = parent->children;
    parent->children = node;
}

// Parse next path component
static const char* next_component(const char* path, char* component, size_t max_len) {
    // Skip leading slashes
    while (*path == '/') path++;
    
    if (!*path) {
        component[0] = '\0';
        return nullptr;
    }
    
    size_t i = 0;
    while (*path && *path != '/' && i < max_len - 1) {
        component[i++] = *path++;
    }
    component[i] = '\0';
    
    return path;
}

// Find child by name
static FSNode* find_child(FSNode* dir, const char* name) {
    if (!dir || dir->type != FileType::DIRECTORY) return nullptr;
    
    FSNode* child = dir->children;
    while (child) {
        if (str_cmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }
    return nullptr;
}

// ============================================================================
// Public API
// ============================================================================

void init() {
    // Clear all nodes
    mem_set(g_nodes, 0, sizeof(g_nodes));
    
    // Create root directory
    g_root = alloc_node();
    if (!g_root) {
        uart::puts("[ramfs] Failed to create root directory!\n");
        return;
    }
    
    str_copy(g_root->name, "/", MAX_FILENAME);
    g_root->type = FileType::DIRECTORY;
    g_root->parent = g_root;  // Root is its own parent
    
    // Set current directory to root
    g_cwd = g_root;
    str_copy(g_cwd_path, "/", MAX_PATH);
    
    uart::puts("[ramfs] RAM filesystem initialized\n");
}

FSNode* get_root() {
    return g_root;
}

FSNode* get_cwd() {
    return g_cwd;
}

const char* get_cwd_path() {
    return g_cwd_path;
}

// Update cwd_path based on current g_cwd
static void update_cwd_path() {
    if (g_cwd == g_root) {
        str_copy(g_cwd_path, "/", MAX_PATH);
        return;
    }
    
    // Build path by traversing up
    char temp[MAX_PATH];
    char result[MAX_PATH];
    result[0] = '\0';
    
    FSNode* node = g_cwd;
    while (node && node != g_root) {
        str_copy(temp, "/", MAX_PATH);
        size_t len = str_len(temp);
        size_t name_len = str_len(node->name);
        if (len + name_len < MAX_PATH - 1) {
            for (size_t i = 0; i < name_len; i++) {
                temp[len + i] = node->name[i];
            }
            temp[len + name_len] = '\0';
        }
        
        // Prepend to result
        size_t result_len = str_len(result);
        size_t temp_len = str_len(temp);
        if (temp_len + result_len < MAX_PATH - 1) {
            // Shift result right
            for (int i = result_len; i >= 0; i--) {
                result[temp_len + i] = result[i];
            }
            // Copy temp to front
            for (size_t i = 0; i < temp_len; i++) {
                result[i] = temp[i];
            }
        }
        
        node = node->parent;
    }
    
    if (result[0] == '\0') {
        str_copy(g_cwd_path, "/", MAX_PATH);
    } else {
        str_copy(g_cwd_path, result, MAX_PATH);
    }
}

FSNode* resolve_path(const char* path) {
    if (!path || !*path) return g_cwd;
    
    FSNode* current;
    
    // Absolute or relative path?
    if (path[0] == '/') {
        current = g_root;
        path++;
    } else {
        current = g_cwd;
    }
    
    char component[MAX_FILENAME];
    
    while (path && *path) {
        path = next_component(path, component, MAX_FILENAME);
        
        if (component[0] == '\0') break;
        
        if (str_cmp(component, ".") == 0) {
            // Current directory, no change
            continue;
        }
        
        if (str_cmp(component, "..") == 0) {
            // Parent directory
            if (current->parent) {
                current = current->parent;
            }
            continue;
        }
        
        // Find child
        FSNode* child = find_child(current, component);
        if (!child) {
            return nullptr;  // Path not found
        }
        current = child;
    }
    
    return current;
}

bool set_cwd(const char* path) {
    FSNode* node = resolve_path(path);
    if (!node || node->type != FileType::DIRECTORY) {
        return false;
    }
    
    g_cwd = node;
    update_cwd_path();
    return true;
}

FSNode* create_file(const char* path) {
    if (!path || !*path) return nullptr;
    
    // Find parent directory and filename
    char parent_path[MAX_PATH];
    char filename[MAX_FILENAME];
    
    // Find last slash
    const char* last_slash = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    FSNode* parent;
    if (last_slash) {
        size_t parent_len = last_slash - path;
        if (parent_len == 0) {
            parent = g_root;
        } else {
            if (parent_len >= MAX_PATH) return nullptr;
            mem_copy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
            parent = resolve_path(parent_path);
        }
        str_copy(filename, last_slash + 1, MAX_FILENAME);
    } else {
        parent = g_cwd;
        str_copy(filename, path, MAX_FILENAME);
    }
    
    if (!parent || parent->type != FileType::DIRECTORY) {
        return nullptr;
    }
    
    // Check if file already exists
    if (find_child(parent, filename)) {
        return nullptr;  // Already exists
    }
    
    // Create new file node
    FSNode* node = alloc_node();
    if (!node) return nullptr;
    
    str_copy(node->name, filename, MAX_FILENAME);
    node->type = FileType::FILE;
    node->size = 0;
    node->data = nullptr;
    
    link_node(parent, node);
    
    return node;
}

FSNode* open_file(const char* path) {
    FSNode* node = resolve_path(path);
    if (!node || node->type != FileType::FILE) {
        return nullptr;
    }
    return node;
}

bool delete_file(const char* path) {
    FSNode* node = resolve_path(path);
    if (!node || node->type != FileType::FILE) {
        return false;
    }
    
    unlink_node(node);
    free_node(node);
    return true;
}

size_t read_file(FSNode* node, uint8_t* buffer, size_t offset, size_t count) {
    if (!node || node->type != FileType::FILE || !buffer) {
        return 0;
    }
    
    if (offset >= node->size) {
        return 0;
    }
    
    size_t available = node->size - offset;
    size_t to_read = (count < available) ? count : available;
    
    if (node->data) {
        mem_copy(buffer, node->data + offset, to_read);
    }
    
    return to_read;
}


size_t write_file(FSNode* node, const uint8_t* buffer, size_t offset, size_t count) {
    if (!node || node->type != FileType::FILE || !buffer) {
        return 0;
    }
    
    size_t new_size = offset + count;
    if (new_size > MAX_FILE_SIZE) {
        new_size = MAX_FILE_SIZE;
        count = new_size - offset;
    }
    
    // Allocate or reallocate data buffer if needed
    if (new_size > 0 && (!node->data || new_size > node->size)) {
        size_t old_pages = node->data ? (node->size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE : 0;
        size_t new_pages = (new_size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        
        if (new_pages > old_pages) {
            uint8_t* new_data = static_cast<uint8_t*>(memory::alloc_pages(new_pages));
            if (!new_data) {
                return 0;  // Out of memory
            }
            
            mem_set(new_data, 0, new_pages * memory::PAGE_SIZE);
            
            if (node->data) {
                mem_copy(new_data, node->data, node->size);
                memory::free_pages(node->data, old_pages);
            }
            
            node->data = new_data;
        }
    }
    
    // Write data
    if (node->data && count > 0) {
        mem_copy(node->data + offset, buffer, count);
    }
    
    if (new_size > node->size) {
        node->size = new_size;
    }
    
    return count;
}

bool truncate_file(FSNode* node, size_t size) {
    if (!node || node->type != FileType::FILE) {
        return false;
    }
    
    if (size > MAX_FILE_SIZE) {
        size = MAX_FILE_SIZE;
    }
    
    if (size == 0) {
        // Free all data
        if (node->data) {
            size_t pages = (node->size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
            memory::free_pages(node->data, pages);
            node->data = nullptr;
        }
        node->size = 0;
    } else if (size < node->size) {
        // Just update size (keep allocated memory)
        node->size = size;
    } else if (size > node->size) {
        // Extend file with zeros
        size_t old_size = node->size;
        size_t new_pages = (size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE;
        size_t old_pages = node->data ? (old_size + memory::PAGE_SIZE - 1) / memory::PAGE_SIZE : 0;
        
        if (new_pages > old_pages) {
            uint8_t* new_data = static_cast<uint8_t*>(memory::alloc_pages(new_pages));
            if (!new_data) return false;
            
            mem_set(new_data, 0, new_pages * memory::PAGE_SIZE);
            
            if (node->data) {
                mem_copy(new_data, node->data, old_size);
                memory::free_pages(node->data, old_pages);
            }
            
            node->data = new_data;
        }
        
        node->size = size;
    }
    
    return true;
}

FSNode* create_dir(const char* path) {
    if (!path || !*path) return nullptr;
    
    // Find parent directory and dirname
    char parent_path[MAX_PATH];
    char dirname[MAX_FILENAME];
    
    // Find last slash
    const char* last_slash = nullptr;
    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    
    FSNode* parent;
    if (last_slash) {
        size_t parent_len = last_slash - path;
        if (parent_len == 0) {
            parent = g_root;
        } else {
            if (parent_len >= MAX_PATH) return nullptr;
            mem_copy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
            parent = resolve_path(parent_path);
        }
        str_copy(dirname, last_slash + 1, MAX_FILENAME);
    } else {
        parent = g_cwd;
        str_copy(dirname, path, MAX_FILENAME);
    }
    
    if (!parent || parent->type != FileType::DIRECTORY) {
        return nullptr;
    }
    
    // Check if already exists
    if (find_child(parent, dirname)) {
        return nullptr;
    }
    
    // Create new directory node
    FSNode* node = alloc_node();
    if (!node) return nullptr;
    
    str_copy(node->name, dirname, MAX_FILENAME);
    node->type = FileType::DIRECTORY;
    node->size = 0;
    node->data = nullptr;
    node->children = nullptr;
    
    link_node(parent, node);
    
    return node;
}

// Recursively delete directory contents
static void delete_contents(FSNode* dir) {
    if (!dir || dir->type != FileType::DIRECTORY) return;
    
    FSNode* child = dir->children;
    while (child) {
        FSNode* next = child->next;
        
        if (child->type == FileType::DIRECTORY) {
            delete_contents(child);
        }
        
        child->parent = nullptr;
        child->next = nullptr;
        free_node(child);
        
        child = next;
    }
    
    dir->children = nullptr;
}

bool delete_dir(const char* path, bool recursive) {
    FSNode* node = resolve_path(path);
    if (!node || node->type != FileType::DIRECTORY) {
        return false;
    }
    
    // Can't delete root
    if (node == g_root) {
        return false;
    }
    
    // Check if empty (unless recursive)
    if (!recursive && node->children) {
        return false;  // Directory not empty
    }
    
    // If recursive, delete all contents first
    if (recursive) {
        delete_contents(node);
    }
    
    // If cwd is this directory or a child, move to parent
    FSNode* check = g_cwd;
    while (check && check != g_root) {
        if (check == node) {
            g_cwd = node->parent;
            update_cwd_path();
            break;
        }
        check = check->parent;
    }
    
    unlink_node(node);
    free_node(node);
    return true;
}

FSNode* open_dir(const char* path) {
    FSNode* node = resolve_path(path);
    if (!node || node->type != FileType::DIRECTORY) {
        return nullptr;
    }
    return node;
}

bool get_full_path(FSNode* node, char* buffer, size_t bufsize) {
    if (!node || !buffer || bufsize == 0) return false;
    
    if (node == g_root) {
        str_copy(buffer, "/", bufsize);
        return true;
    }
    
    // Build path by traversing up
    char temp[MAX_PATH];
    buffer[0] = '\0';
    
    while (node && node != g_root) {
        str_copy(temp, "/", MAX_PATH);
        size_t len = str_len(temp);
        size_t name_len = str_len(node->name);
        
        if (len + name_len < MAX_PATH - 1) {
            for (size_t i = 0; i < name_len; i++) {
                temp[len + i] = node->name[i];
            }
            temp[len + name_len] = '\0';
        }
        
        // Prepend to buffer
        size_t buf_len = str_len(buffer);
        size_t temp_len = str_len(temp);
        
        if (temp_len + buf_len < bufsize - 1) {
            // Shift buffer right
            for (int i = buf_len; i >= 0; i--) {
                buffer[temp_len + i] = buffer[i];
            }
            // Copy temp to front
            for (size_t i = 0; i < temp_len; i++) {
                buffer[i] = temp[i];
            }
        }
        
        node = node->parent;
    }
    
    if (buffer[0] == '\0') {
        str_copy(buffer, "/", bufsize);
    }
    
    return true;
}

bool dir_open(DirIterator* iter, const char* path) {
    if (!iter) return false;
    
    FSNode* dir = resolve_path(path);
    if (!dir || dir->type != FileType::DIRECTORY) {
        return false;
    }
    
    iter->dir = dir;
    iter->current = dir->children;
    return true;
}

FSNode* dir_next(DirIterator* iter) {
    if (!iter || !iter->current) return nullptr;
    
    FSNode* result = iter->current;
    iter->current = iter->current->next;
    return result;
}

FSStats get_stats() {
    FSStats stats = {0, 0, 0, 0};
    
    stats.total_nodes = MAX_FILES;
    stats.total_bytes = MAX_FILES * MAX_FILE_SIZE;
    
    for (size_t i = 0; i < MAX_FILES; i++) {
        if (g_nodes[i].in_use) {
            stats.used_nodes++;
            if (g_nodes[i].type == FileType::FILE) {
                stats.used_bytes += g_nodes[i].size;
            }
        }
    }
    
    return stats;
}

} // namespace ramfs
