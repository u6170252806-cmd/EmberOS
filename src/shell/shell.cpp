/*
 * EmberOS Shell Implementation
 * Command-line interface with line editing and history
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6
 */

#include "shell.h"
#include "uart.h"
#include "ramfs.h"

namespace shell {

// ============================================================================
// Internal Data Structures
// ============================================================================

/*
 * Command entry in the registry
 */
struct Command {
    const char* name;
    const char* help;
    CommandHandler handler;
};

/*
 * Line editor state
 * Requirements: 5.6
 */
struct LineEditor {
    char buffer[MAX_CMD_LEN];
    size_t cursor;      // Current cursor position
    size_t length;      // Current line length
};

/*
 * Command history
 * Requirements: 5.5
 */
struct History {
    char entries[HISTORY_SIZE][MAX_CMD_LEN];
    size_t count;       // Number of entries in history
    int current;        // Current position when navigating (-1 = not navigating)
};

/*
 * Alias entry
 */
struct Alias {
    char name[MAX_VAR_NAME];
    char value[MAX_CMD_LEN];
    bool active;
};

/*
 * Environment variable entry
 */
struct EnvVar {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
    bool active;
};

// ============================================================================
// Global State
// ============================================================================

static Command g_commands[MAX_COMMANDS];
static size_t g_command_count = 0;

static LineEditor g_editor;
static History g_history;
static Alias g_aliases[MAX_ALIASES];
static EnvVar g_env_vars[MAX_ENV_VARS];

// Process table
static ProcessInfo g_processes[MAX_PROCESSES];
static int g_next_pid = 1;
static int g_current_pid = 0;

// ============================================================================
// String Utilities (no standard library)
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
    char* p = static_cast<char*>(ptr);
    for (size_t i = 0; i < size; i++) {
        p[i] = static_cast<char>(value);
    }
}

static void mem_move(void* dst, const void* src, size_t size) {
    char* d = static_cast<char*>(dst);
    const char* s = static_cast<const char*>(src);
    
    if (d < s) {
        for (size_t i = 0; i < size; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = size; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
}

// ============================================================================
// Command Parser Implementation
// Requirements: 5.2
// ============================================================================

/*
 * Check if character is whitespace
 */
static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/*
 * Parse a command line into argc/argv
 * Tokenizes the input string by whitespace
 * Modifies cmdline in place (inserts null terminators)
 * Returns number of arguments (argc)
 * Requirements: 5.2
 */
int parse_command(char* cmdline, char* argv[], int max_args) {
    int argc = 0;
    char* p = cmdline;
    
    while (*p && argc < max_args) {
        // Skip leading whitespace
        while (*p && is_whitespace(*p)) {
            p++;
        }
        
        if (!*p) break;
        
        // Found start of token
        argv[argc++] = p;
        
        // Find end of token
        while (*p && !is_whitespace(*p)) {
            p++;
        }
        
        // Null-terminate token if not at end of string
        if (*p) {
            *p++ = '\0';
        }
    }
    
    return argc;
}

// ============================================================================
// Command Registry Implementation
// Requirements: 5.3, 5.4
// ============================================================================

/*
 * Register a command with the shell
 * Requirements: 5.3
 */
void register_command(const char* name, const char* help, CommandHandler handler) {
    if (g_command_count >= MAX_COMMANDS) {
        uart::puts("[shell] Error: command registry full\n");
        return;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < g_command_count; i++) {
        if (str_cmp(g_commands[i].name, name) == 0) {
            uart::printf("[shell] Warning: command '%s' already registered\n", name);
            return;
        }
    }
    
    g_commands[g_command_count].name = name;
    g_commands[g_command_count].help = help;
    g_commands[g_command_count].handler = handler;
    g_command_count++;
}

/*
 * Look up a command by name
 * Returns the command handler, or nullptr if not found
 * Requirements: 5.3, 5.4
 */
CommandHandler lookup_command(const char* name) {
    for (size_t i = 0; i < g_command_count; i++) {
        if (str_cmp(g_commands[i].name, name) == 0) {
            return g_commands[i].handler;
        }
    }
    return nullptr;
}

/*
 * Get help text for a command
 */
const char* get_command_help(const char* name) {
    for (size_t i = 0; i < g_command_count; i++) {
        if (str_cmp(g_commands[i].name, name) == 0) {
            return g_commands[i].help;
        }
    }
    return nullptr;
}

/*
 * Get the number of registered commands
 */
int get_command_count() {
    return static_cast<int>(g_command_count);
}

/*
 * Get command name by index
 */
const char* get_command_name(int index) {
    if (index >= 0 && static_cast<size_t>(index) < g_command_count) {
        return g_commands[index].name;
    }
    return nullptr;
}

// ============================================================================
// Line Editor Implementation
// Requirements: 5.6
// ============================================================================

// ANSI escape codes
constexpr char ESC = '\x1b';
constexpr char BACKSPACE = '\x7f';
constexpr char CTRL_C = '\x03';
constexpr char CTRL_D = '\x04';
constexpr char CTRL_U = '\x15';

/*
 * Clear the line editor buffer
 */
static void editor_clear() {
    mem_set(g_editor.buffer, 0, MAX_CMD_LEN);
    g_editor.cursor = 0;
    g_editor.length = 0;
}

/*
 * Redraw the current line from cursor position
 */
static void editor_redraw_from_cursor() {
    // Print characters from cursor to end
    for (size_t i = g_editor.cursor; i < g_editor.length; i++) {
        uart::putc(g_editor.buffer[i]);
    }
    // Clear any remaining characters (in case of deletion)
    uart::putc(' ');
    // Move cursor back to correct position
    for (size_t i = g_editor.cursor; i <= g_editor.length; i++) {
        uart::putc('\b');
    }
}

/*
 * Insert a character at cursor position
 */
static void editor_insert_char(char c) {
    if (g_editor.length >= MAX_CMD_LEN - 1) {
        return;  // Buffer full
    }
    
    // Shift characters right to make room
    if (g_editor.cursor < g_editor.length) {
        mem_move(&g_editor.buffer[g_editor.cursor + 1],
                 &g_editor.buffer[g_editor.cursor],
                 g_editor.length - g_editor.cursor);
    }
    
    // Insert character
    g_editor.buffer[g_editor.cursor] = c;
    g_editor.cursor++;
    g_editor.length++;
    g_editor.buffer[g_editor.length] = '\0';
    
    // Echo character and redraw rest of line
    uart::putc(c);
    if (g_editor.cursor < g_editor.length) {
        editor_redraw_from_cursor();
    }
}

/*
 * Delete character before cursor (backspace)
 */
static void editor_backspace() {
    if (g_editor.cursor == 0) {
        return;  // Nothing to delete
    }
    
    // Shift characters left
    mem_move(&g_editor.buffer[g_editor.cursor - 1],
             &g_editor.buffer[g_editor.cursor],
             g_editor.length - g_editor.cursor + 1);
    
    g_editor.cursor--;
    g_editor.length--;
    
    // Move cursor back and redraw
    uart::putc('\b');
    editor_redraw_from_cursor();
}

/*
 * Delete character at cursor (delete key)
 */
static void editor_delete() {
    if (g_editor.cursor >= g_editor.length) {
        return;  // Nothing to delete
    }
    
    // Shift characters left
    mem_move(&g_editor.buffer[g_editor.cursor],
             &g_editor.buffer[g_editor.cursor + 1],
             g_editor.length - g_editor.cursor);
    
    g_editor.length--;
    
    // Redraw from cursor
    editor_redraw_from_cursor();
}

/*
 * Move cursor left
 */
static void editor_cursor_left() {
    if (g_editor.cursor > 0) {
        g_editor.cursor--;
        uart::putc('\b');
    }
}

/*
 * Move cursor right
 */
static void editor_cursor_right() {
    if (g_editor.cursor < g_editor.length) {
        uart::putc(g_editor.buffer[g_editor.cursor]);
        g_editor.cursor++;
    }
}

/*
 * Move cursor to beginning of line
 */
static void editor_cursor_home() {
    while (g_editor.cursor > 0) {
        uart::putc('\b');
        g_editor.cursor--;
    }
}

/*
 * Move cursor to end of line
 */
static void editor_cursor_end() {
    while (g_editor.cursor < g_editor.length) {
        uart::putc(g_editor.buffer[g_editor.cursor]);
        g_editor.cursor++;
    }
}

/*
 * Clear the entire line
 */
static void editor_clear_line() {
    // Move to beginning
    editor_cursor_home();
    // Clear display
    for (size_t i = 0; i < g_editor.length; i++) {
        uart::putc(' ');
    }
    for (size_t i = 0; i < g_editor.length; i++) {
        uart::putc('\b');
    }
    // Reset buffer
    editor_clear();
}

/*
 * Replace line with history entry
 */
static void editor_set_line(const char* line) {
    editor_clear_line();
    str_copy(g_editor.buffer, line, MAX_CMD_LEN);
    g_editor.length = str_len(g_editor.buffer);
    g_editor.cursor = g_editor.length;
    uart::puts(g_editor.buffer);
}

// ============================================================================
// Command History Implementation
// Requirements: 5.5
// ============================================================================

/*
 * Initialize history
 */
static void history_init() {
    mem_set(&g_history, 0, sizeof(g_history));
    g_history.count = 0;
    g_history.current = -1;
}

/*
 * Add command to history
 */
static void history_add(const char* cmd) {
    // Don't add empty commands
    if (!cmd || !cmd[0]) {
        return;
    }
    
    // Don't add duplicate of last command
    if (g_history.count > 0) {
        size_t last = (g_history.count - 1) % HISTORY_SIZE;
        if (str_cmp(g_history.entries[last], cmd) == 0) {
            return;
        }
    }
    
    // Add to history (circular buffer)
    size_t index = g_history.count % HISTORY_SIZE;
    str_copy(g_history.entries[index], cmd, MAX_CMD_LEN);
    
    if (g_history.count < HISTORY_SIZE) {
        g_history.count++;
    }
    
    // Reset navigation position
    g_history.current = -1;
}

/*
 * Navigate to previous history entry (up arrow)
 */
static const char* history_prev() {
    if (g_history.count == 0) {
        return nullptr;
    }
    
    if (g_history.current == -1) {
        // Start from most recent
        g_history.current = static_cast<int>(g_history.count) - 1;
    } else if (g_history.current > 0 && 
               static_cast<size_t>(g_history.current) > g_history.count - HISTORY_SIZE) {
        g_history.current--;
    } else {
        return nullptr;  // At oldest entry
    }
    
    size_t index = static_cast<size_t>(g_history.current) % HISTORY_SIZE;
    return g_history.entries[index];
}

/*
 * Navigate to next history entry (down arrow)
 */
static const char* history_next() {
    if (g_history.current == -1) {
        return nullptr;
    }
    
    g_history.current++;
    
    if (static_cast<size_t>(g_history.current) >= g_history.count) {
        g_history.current = -1;
        return "";  // Return empty string for new command
    }
    
    size_t index = static_cast<size_t>(g_history.current) % HISTORY_SIZE;
    return g_history.entries[index];
}

/*
 * Reset history navigation
 */
static void history_reset_nav() {
    g_history.current = -1;
}

/*
 * Get history entry by index
 */
const char* get_history_entry(int index) {
    if (index < 0 || static_cast<size_t>(index) >= g_history.count) {
        return nullptr;
    }
    size_t actual_index = static_cast<size_t>(index) % HISTORY_SIZE;
    return g_history.entries[actual_index];
}

/*
 * Get history count
 */
int get_history_count() {
    return static_cast<int>(g_history.count);
}

// ============================================================================
// Alias Management
// ============================================================================

/*
 * Initialize aliases
 */
static void alias_init() {
    for (size_t i = 0; i < MAX_ALIASES; i++) {
        g_aliases[i].active = false;
    }
}

/*
 * Add or update an alias
 */
bool add_alias(const char* name, const char* value) {
    // Check if alias exists
    for (size_t i = 0; i < MAX_ALIASES; i++) {
        if (g_aliases[i].active && str_cmp(g_aliases[i].name, name) == 0) {
            str_copy(g_aliases[i].value, value, MAX_CMD_LEN);
            return true;
        }
    }
    
    // Find empty slot
    for (size_t i = 0; i < MAX_ALIASES; i++) {
        if (!g_aliases[i].active) {
            str_copy(g_aliases[i].name, name, MAX_VAR_NAME);
            str_copy(g_aliases[i].value, value, MAX_CMD_LEN);
            g_aliases[i].active = true;
            return true;
        }
    }
    
    return false;  // No space
}

/*
 * Remove an alias
 */
bool remove_alias(const char* name) {
    for (size_t i = 0; i < MAX_ALIASES; i++) {
        if (g_aliases[i].active && str_cmp(g_aliases[i].name, name) == 0) {
            g_aliases[i].active = false;
            return true;
        }
    }
    return false;
}

/*
 * Get alias value
 */
const char* get_alias(const char* name) {
    for (size_t i = 0; i < MAX_ALIASES; i++) {
        if (g_aliases[i].active && str_cmp(g_aliases[i].name, name) == 0) {
            return g_aliases[i].value;
        }
    }
    return nullptr;
}

// ============================================================================
// Environment Variable Management
// ============================================================================

/*
 * Initialize environment
 */
static void env_init() {
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        g_env_vars[i].active = false;
    }
    
    // Set default environment variables
    set_env("USER", "root");
    set_env("HOME", "/");
    set_env("SHELL", "/bin/ember");
    set_env("PATH", "/bin");
    set_env("HOSTNAME", "ember");
}

/*
 * Set environment variable
 */
bool set_env(const char* name, const char* value) {
    // Check if var exists
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active && str_cmp(g_env_vars[i].name, name) == 0) {
            str_copy(g_env_vars[i].value, value, MAX_VAR_VALUE);
            return true;
        }
    }
    
    // Find empty slot
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (!g_env_vars[i].active) {
            str_copy(g_env_vars[i].name, name, MAX_VAR_NAME);
            str_copy(g_env_vars[i].value, value, MAX_VAR_VALUE);
            g_env_vars[i].active = true;
            return true;
        }
    }
    
    return false;
}

/*
 * Get environment variable
 */
const char* get_env(const char* name) {
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active && str_cmp(g_env_vars[i].name, name) == 0) {
            return g_env_vars[i].value;
        }
    }
    return nullptr;
}

/*
 * Unset environment variable
 */
bool unset_env(const char* name) {
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active && str_cmp(g_env_vars[i].name, name) == 0) {
            g_env_vars[i].active = false;
            return true;
        }
    }
    return false;
}

/*
 * Get environment variable count
 */
int get_env_count() {
    int count = 0;
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active) count++;
    }
    return count;
}

/*
 * Get environment variable name by index
 */
const char* get_env_name(int index) {
    int count = 0;
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active) {
            if (count == index) return g_env_vars[i].name;
            count++;
        }
    }
    return nullptr;
}

/*
 * Get environment variable value by index
 */
const char* get_env_value(int index) {
    int count = 0;
    for (size_t i = 0; i < MAX_ENV_VARS; i++) {
        if (g_env_vars[i].active) {
            if (count == index) return g_env_vars[i].value;
            count++;
        }
    }
    return nullptr;
}

// ============================================================================
// Input Handling
// ============================================================================

/*
 * Process escape sequence (arrow keys, etc.)
 */
static void process_escape_sequence() {
    // Read next character
    char c = uart::getc();
    
    if (c != '[') {
        return;  // Not a CSI sequence
    }
    
    // Read the command character
    c = uart::getc();
    
    switch (c) {
        case 'A':  // Up arrow
            {
                const char* prev = history_prev();
                if (prev) {
                    editor_set_line(prev);
                }
            }
            break;
            
        case 'B':  // Down arrow
            {
                const char* next = history_next();
                if (next) {
                    editor_set_line(next);
                }
            }
            break;
            
        case 'C':  // Right arrow
            editor_cursor_right();
            break;
            
        case 'D':  // Left arrow
            editor_cursor_left();
            break;
            
        case 'H':  // Home
            editor_cursor_home();
            break;
            
        case 'F':  // End
            editor_cursor_end();
            break;
            
        case '3':  // Delete key (sends ESC[3~)
            c = uart::getc();  // Read the '~'
            if (c == '~') {
                editor_delete();
            }
            break;
            
        default:
            // Unknown sequence, ignore
            break;
    }
}

/*
 * Read a line of input with editing support
 * Returns true if a line was read, false on EOF/Ctrl-D
 */
static bool read_line() {
    editor_clear();
    history_reset_nav();
    
    while (true) {
        char c = uart::getc();
        
        switch (c) {
            case '\r':
            case '\n':
                // End of line
                uart::puts("\r\n");
                return true;
                
            case ESC:
                // Escape sequence
                process_escape_sequence();
                break;
                
            case BACKSPACE:
            case '\b':
                // Backspace
                editor_backspace();
                break;
                
            case CTRL_C:
                // Cancel current line
                uart::puts("^C\r\n");
                editor_clear();
                return true;
                
            case CTRL_D:
                // EOF (only if line is empty)
                if (g_editor.length == 0) {
                    uart::puts("\r\n");
                    return false;
                }
                break;
                
            case CTRL_U:
                // Clear line
                editor_clear_line();
                break;
                
            default:
                // Regular character
                if (c >= 0x20 && c < 0x7f) {
                    editor_insert_char(c);
                }
                break;
        }
    }
}

// ============================================================================
// Shell Main Loop
// Requirements: 5.1
// ============================================================================

/*
 * Display the shell prompt
 */
static void display_prompt() {
    uart::puts("ember:");
    // Show current directory (abbreviated if too long)
    const char* cwd = ramfs::get_cwd_path();
    uart::puts(cwd);
    uart::puts("> ");
}

/*
 * Execute a command line
 */
static void execute_command(char* cmdline) {
    char* argv[MAX_ARGS];
    int argc = parse_command(cmdline, argv, MAX_ARGS);
    
    if (argc == 0) {
        return;  // Empty command
    }
    
    // Check for alias expansion
    const char* alias_value = get_alias(argv[0]);
    if (alias_value) {
        // Create expanded command line
        static char expanded[MAX_CMD_LEN];
        str_copy(expanded, alias_value, MAX_CMD_LEN);
        
        // Append remaining arguments
        for (int i = 1; i < argc; i++) {
            size_t len = str_len(expanded);
            if (len < MAX_CMD_LEN - 2) {
                expanded[len] = ' ';
                str_copy(expanded + len + 1, argv[i], MAX_CMD_LEN - len - 1);
            }
        }
        
        // Re-parse expanded command
        argc = parse_command(expanded, argv, MAX_ARGS);
        if (argc == 0) return;
    }
    
    // Look up command
    CommandHandler handler = lookup_command(argv[0]);
    
    if (handler) {
        handler(argc, argv);
    } else {
        // Requirements: 5.4 - display error for invalid command
        uart::printf("Unknown command: %s. Type 'help' for available commands.\n", argv[0]);
    }
}

// ============================================================================
// Process Management
// ============================================================================

/*
 * Initialize process table
 */
static void process_init() {
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        g_processes[i].state = ProcessState::FREE;
        g_processes[i].pid = 0;
    }
    g_next_pid = 1;
    g_current_pid = 0;
}

/*
 * Initialize shell subsystem
 * Requirements: 5.1
 */
void init() {
    // Initialize line editor
    editor_clear();
    
    // Initialize history
    history_init();
    
    // Initialize aliases
    alias_init();
    
    // Initialize environment
    env_init();
    
    // Initialize process table
    process_init();
    
    // Reset command registry
    g_command_count = 0;
    
    uart::puts("[shell] Shell initialized\n");
}

/*
 * Run shell main loop (never returns)
 * Requirements: 5.1
 */
void run() {
    uart::puts("\nEmberOS Shell - Type 'help' for available commands\n\n");
    
    while (true) {
        display_prompt();
        
        if (!read_line()) {
            // EOF received
            uart::puts("logout\n");
            break;
        }
        
        // Add to history if non-empty
        if (g_editor.buffer[0]) {
            history_add(g_editor.buffer);
        }
        
        // Execute command
        execute_command(g_editor.buffer);
    }
}

/*
 * Create a new process
 */
int create_process(const char* name) {
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].state == ProcessState::FREE) {
            g_processes[i].pid = g_next_pid++;
            g_processes[i].state = ProcessState::RUNNING;
            str_copy(g_processes[i].name, name, 32);
            g_processes[i].start_time = 0;  // Will be set by caller
            g_processes[i].cpu_time = 0;
            g_processes[i].memory = 0;
            return g_processes[i].pid;
        }
    }
    return -1;  // No free slots
}

/*
 * Destroy a process
 */
void destroy_process(int pid) {
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].pid == pid && g_processes[i].state != ProcessState::FREE) {
            g_processes[i].state = ProcessState::FREE;
            g_processes[i].pid = 0;
            return;
        }
    }
}

/*
 * Update process state
 */
void update_process(int pid, ProcessState state, uint32_t cpu_delta) {
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].pid == pid) {
            g_processes[i].state = state;
            g_processes[i].cpu_time += cpu_delta;
            return;
        }
    }
}

/*
 * Get process by PID
 */
ProcessInfo* get_process(int pid) {
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].pid == pid && g_processes[i].state != ProcessState::FREE) {
            return &g_processes[i];
        }
    }
    return nullptr;
}

/*
 * Get process count
 */
int get_process_count() {
    int count = 0;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].state != ProcessState::FREE) {
            count++;
        }
    }
    return count;
}

/*
 * Get process by index
 */
ProcessInfo* get_process_by_index(int index) {
    int count = 0;
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        if (g_processes[i].state != ProcessState::FREE) {
            if (count == index) {
                return &g_processes[i];
            }
            count++;
        }
    }
    return nullptr;
}

/*
 * Get current PID
 */
int get_current_pid() {
    return g_current_pid;
}

/*
 * Set current PID
 */
void set_current_pid(int pid) {
    g_current_pid = pid;
}

} // namespace shell
