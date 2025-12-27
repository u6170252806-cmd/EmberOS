/*
 * EmberOS RAM Filesystem
 * In-memory filesystem for storing files and directories
 */

#ifndef EMBEROS_RAMFS_H
#define EMBEROS_RAMFS_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using int32_t = int;

namespace ramfs {

// Filesystem limits
constexpr size_t MAX_FILENAME = 64;
constexpr size_t MAX_PATH = 256;
constexpr size_t MAX_FILES = 128;
constexpr size_t MAX_FILE_SIZE = 65536;  // 64KB per file
constexpr size_t MAX_DIR_ENTRIES = 32;

// File types
enum class FileType {
    FILE,
    DIRECTORY
};

// Forward declaration
struct DirEntry;

// File/Directory node
struct FSNode {
    char name[MAX_FILENAME];
    FileType type;
    size_t size;              // For files: content size, for dirs: entry count
    uint8_t* data;            // For files: content, for dirs: nullptr
    FSNode* parent;           // Parent directory
    FSNode* children;         // For directories: first child
    FSNode* next;             // Next sibling in same directory
    bool in_use;
};

// Initialize the filesystem
void init();

// Path operations
FSNode* get_root();
FSNode* get_cwd();
bool set_cwd(const char* path);
const char* get_cwd_path();

// File operations
FSNode* create_file(const char* path);
FSNode* open_file(const char* path);
bool delete_file(const char* path);
size_t read_file(FSNode* node, uint8_t* buffer, size_t offset, size_t count);
size_t write_file(FSNode* node, const uint8_t* buffer, size_t offset, size_t count);
bool truncate_file(FSNode* node, size_t size);

// Directory operations
FSNode* create_dir(const char* path);
bool delete_dir(const char* path, bool recursive);
FSNode* open_dir(const char* path);

// Node operations
FSNode* resolve_path(const char* path);
bool get_full_path(FSNode* node, char* buffer, size_t bufsize);

// Listing
struct DirIterator {
    FSNode* dir;
    FSNode* current;
};

bool dir_open(DirIterator* iter, const char* path);
FSNode* dir_next(DirIterator* iter);

// Stats
struct FSStats {
    size_t total_nodes;
    size_t used_nodes;
    size_t total_bytes;
    size_t used_bytes;
};

FSStats get_stats();

} // namespace ramfs

#endif // EMBEROS_RAMFS_H
