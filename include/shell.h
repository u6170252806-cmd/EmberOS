/*
 * EmberOS Shell Interface
 * Command-line interface with line editing and history
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6
 */

#ifndef EMBEROS_SHELL_H
#define EMBEROS_SHELL_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace shell {

// Shell constants
constexpr size_t MAX_CMD_LEN = 256;
constexpr size_t MAX_ARGS = 16;
constexpr size_t HISTORY_SIZE = 20;
constexpr size_t MAX_COMMANDS = 48;
constexpr size_t MAX_ALIASES = 16;
constexpr size_t MAX_ENV_VARS = 32;
constexpr size_t MAX_VAR_NAME = 32;
constexpr size_t MAX_VAR_VALUE = 128;
constexpr size_t MAX_PROCESSES = 16;

/*
 * Process state
 */
enum class ProcessState {
    FREE = 0,
    RUNNING,
    SLEEPING,
    STOPPED
};

/*
 * Process info structure
 */
struct ProcessInfo {
    int pid;
    ProcessState state;
    char name[32];
    uint64_t start_time;    // When process started (ms)
    uint64_t cpu_time;      // CPU time used (ms)
    uint32_t memory;        // Memory usage (bytes)
};

/*
 * Command handler function type
 * argc: number of arguments (including command name)
 * argv: array of argument strings
 */
using CommandHandler = void (*)(int argc, char* argv[]);

/*
 * Initialize shell subsystem
 * Registers built-in commands and prepares line editor
 * Requirements: 5.1
 */
void init();

/*
 * Run shell main loop (never returns)
 * Displays prompt, reads input, parses and executes commands
 * Requirements: 5.1
 */
void run();

/*
 * Register a command with the shell
 * name: command name (must be unique)
 * help: help text description
 * handler: function to call when command is executed
 * Requirements: 5.3
 */
void register_command(const char* name, const char* help, CommandHandler handler);

/*
 * Parse a command line into argc/argv
 * Tokenizes the input string by whitespace
 * Returns number of arguments (argc)
 * Requirements: 5.2
 */
int parse_command(char* cmdline, char* argv[], int max_args);

/*
 * Look up a command by name
 * Returns the command handler, or nullptr if not found
 * Requirements: 5.3, 5.4
 */
CommandHandler lookup_command(const char* name);

/*
 * Get help text for a command
 * Returns nullptr if command not found
 */
const char* get_command_help(const char* name);

/*
 * Get the number of registered commands
 */
int get_command_count();

/*
 * Get command name by index (for help listing)
 */
const char* get_command_name(int index);

/*
 * Alias management
 */
bool add_alias(const char* name, const char* value);
bool remove_alias(const char* name);
const char* get_alias(const char* name);

/*
 * Environment variable management
 */
bool set_env(const char* name, const char* value);
const char* get_env(const char* name);
bool unset_env(const char* name);
int get_env_count();
const char* get_env_name(int index);
const char* get_env_value(int index);

/*
 * Get history entry by index (0 = oldest)
 */
const char* get_history_entry(int index);
int get_history_count();

/*
 * Process management
 */
int create_process(const char* name);
void destroy_process(int pid);
void update_process(int pid, ProcessState state, uint32_t cpu_delta);
ProcessInfo* get_process(int pid);
int get_process_count();
ProcessInfo* get_process_by_index(int index);
int get_current_pid();
void set_current_pid(int pid);

} // namespace shell

#endif // EMBEROS_SHELL_H
