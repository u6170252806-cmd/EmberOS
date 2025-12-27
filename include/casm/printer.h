/*
 * EmberOS CASM Pretty Printer Header
 * Converts AST back to C.ASM source text
 * 
 * Requirements: 8.7
 */

#ifndef EMBEROS_CASM_PRINTER_H
#define EMBEROS_CASM_PRINTER_H

#include "casm/parser.h"

namespace casm {

/*
 * Maximum output buffer size for printer
 */
constexpr size_t MAX_PRINT_BUFFER = 65536;

/*
 * Printer class for converting AST back to C.ASM source
 * 
 * Usage:
 *   Printer printer;
 *   char output[4096];
 *   printer.print(ast, output, sizeof(output));
 * 
 * Requirements: 8.7
 */
class Printer {
public:
    /*
     * Construct a printer
     */
    Printer();
    
    /*
     * Convert AST back to C.ASM source text
     * 
     * ast: Root AST node to print
     * output: Buffer to write output to
     * max_len: Maximum length of output buffer
     * 
     * Returns: Number of characters written (excluding null terminator)
     */
    size_t print(ASTNode* ast, char* output, size_t max_len);
    
    /*
     * Check if printing encountered an error (buffer overflow)
     */
    bool has_error() const { return has_error_; }

private:
    char* output_;
    size_t max_len_;
    size_t pos_;
    bool has_error_;
    
    // Output helpers
    void write_char(char c);
    void write_string(const char* str);
    void write_string_n(const char* str, size_t len);
    void write_number(int64_t value);
    void write_hex(uint64_t value);
    void write_newline();
    void write_space();
    void write_comma();
    
    // Node printing methods
    void print_node(ASTNode* node);
    void print_program(ASTNode* node);
    void print_label(ASTNode* node);
    void print_instruction(ASTNode* node);
    void print_directive(ASTNode* node);
    void print_operand(ASTNode* node);
    void print_register(ASTNode* node);
    void print_immediate(ASTNode* node);
    void print_label_operand(ASTNode* node);
    void print_memory_operand(ASTNode* node);
    
    // Helper to get register name string
    void write_register_name(int reg_num, bool is_64bit);
};

} // namespace casm

#endif // EMBEROS_CASM_PRINTER_H
