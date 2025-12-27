/*
 * EmberOS Built-in Shell Commands
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.10, 6.13
 */

#ifndef EMBEROS_COMMANDS_H
#define EMBEROS_COMMANDS_H

namespace commands {

/*
 * Register all built-in commands with the shell
 * Must be called after shell::init()
 */
void register_all();

// Basic commands (Requirements: 6.1, 6.2, 6.3, 6.4)
void cmd_help(int argc, char* argv[]);
void cmd_clear(int argc, char* argv[]);
void cmd_echo(int argc, char* argv[]);
void cmd_version(int argc, char* argv[]);

// System info commands (Requirements: 6.5, 6.6, 6.7, 6.10)
void cmd_uptime(int argc, char* argv[]);
void cmd_meminfo(int argc, char* argv[]);
void cmd_cpuinfo(int argc, char* argv[]);
void cmd_date(int argc, char* argv[]);

// System control commands (Requirements: 6.8, 6.9)
void cmd_reboot(int argc, char* argv[]);
void cmd_shutdown(int argc, char* argv[]);

// Utility commands (Requirements: 6.13)
void cmd_hexdump(int argc, char* argv[]);

// CASM assembler command (Requirements: 7.11)
void cmd_casm(int argc, char* argv[]);

} // namespace commands

#endif // EMBEROS_COMMANDS_H
