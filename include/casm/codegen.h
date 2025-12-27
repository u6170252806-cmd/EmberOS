/*
 * EmberOS CASM Code Generator Header
 * Generates ARM64 machine code from AST
 * 
 * Requirements: 7.2, 7.3, 7.4, 7.5, 7.6, 7.8, 7.9, 7.10
 */

#ifndef EMBEROS_CASM_CODEGEN_H
#define EMBEROS_CASM_CODEGEN_H

#include "casm/parser.h"

namespace casm {

/*
 * Maximum number of symbols in symbol table
 * Requirements: 7.6
 */
constexpr int MAX_SYMBOLS = 64;

/*
 * Maximum symbol name length
 */
constexpr int MAX_SYMBOL_NAME = 32;

/*
 * Maximum code buffer size (4KB)
 * Requirements: 7.10
 */
constexpr size_t MAX_CODE_SIZE = 4096;

/*
 * Maximum error message length
 */
constexpr int MAX_CODEGEN_ERROR = 256;

/*
 * Symbol table entry for label tracking
 * Requirements: 7.6
 */
struct Symbol {
    char name[MAX_SYMBOL_NAME];
    uint64_t address;
    bool defined;
    bool is_global;
};

/*
 * Section types for code organization
 * Requirements: 7.8
 */
enum class Section {
    TEXT,   // Code section
    DATA,   // Initialized data
    BSS     // Uninitialized data
};

/*
 * Code Generator class for CASM assembler
 * 
 * Implements two-pass assembly:
 *   Pass 1: Collect all label definitions and calculate addresses
 *   Pass 2: Generate machine code with resolved label references
 * 
 * Usage:
 *   CodeGenerator codegen;
 *   if (codegen.generate(ast)) {
 *       const uint8_t* code = codegen.get_code();
 *       size_t size = codegen.get_code_size();
 *   } else {
 *       const char* error = codegen.get_error();
 *   }
 * 
 * Requirements: 7.2, 7.3, 7.4, 7.5, 7.6, 7.8, 7.9, 7.10
 */
class CodeGenerator {
public:
    /*
     * Construct a code generator
     */
    CodeGenerator();
    
    /*
     * Generate machine code from AST
     * Returns true on success, false on error
     */
    bool generate(ASTNode* ast);
    
    /*
     * Get generated machine code buffer
     */
    const uint8_t* get_code() const { return code_; }
    
    /*
     * Get size of generated code in bytes
     */
    size_t get_code_size() const { return code_offset_; }
    
    /*
     * Get error message if generation failed
     * Requirements: 7.9
     */
    const char* get_error() const { return error_msg_; }
    
    /*
     * Get error line number
     * Requirements: 7.9
     */
    int get_error_line() const { return error_line_; }
    
    /*
     * Check if generation encountered an error
     */
    bool has_error() const { return has_error_; }
    
    /*
     * Reset generator state for reuse
     */
    void reset();
    
    /*
     * Look up a symbol by name
     * Returns nullptr if not found
     */
    const Symbol* lookup_symbol(const char* name, size_t len) const;
    
    /*
     * Get symbol count
     */
    int get_symbol_count() const { return symbol_count_; }
    
    /*
     * Get symbol by index
     */
    const Symbol* get_symbol(int index) const;

private:
    // Code buffer
    uint8_t code_[MAX_CODE_SIZE];
    size_t code_offset_;
    
    // Symbol table
    Symbol symbols_[MAX_SYMBOLS];
    int symbol_count_;
    
    // Current section
    Section current_section_;
    
    // Error state
    bool has_error_;
    char error_msg_[MAX_CODEGEN_ERROR];
    int error_line_;
    
    // Current line for error reporting
    int current_line_;
    
    // Pass tracking
    bool is_first_pass_;
    
    // Peephole optimization state
    uint32_t last_instr_;      // Last emitted instruction
    size_t last_instr_offset_; // Offset of last instruction
    bool has_last_instr_;      // Whether we have a previous instruction
    
    // Error handling
    void error(const char* message);
    void error_at(int line, const char* message);
    
    // Symbol table operations
    Symbol* add_symbol(const char* name, size_t len);
    Symbol* find_symbol(const char* name, size_t len);
    bool define_symbol(const char* name, size_t len, uint64_t address);
    
    // Code emission
    void emit_byte(uint8_t byte);
    void emit_half(uint16_t half);
    void emit_word(uint32_t word);
    void emit_quad(uint64_t quad);
    void align_to(size_t alignment);
    
    // Two-pass assembly
    bool first_pass(ASTNode* ast);
    bool second_pass(ASTNode* ast);
    
    // Node processing
    void process_node(ASTNode* node);
    void process_label(ASTNode* node);
    void process_instruction(ASTNode* node);
    void process_directive(ASTNode* node);
    
    // Instruction encoding
    // Requirements: 7.2, 7.3, 7.4, 7.5
    uint32_t encode_instruction(ASTNode* node);
    uint32_t encode_data_proc(ASTNode* node);
    uint32_t encode_data_proc_imm(ASTNode* node);
    uint32_t encode_load_store(ASTNode* node);
    uint32_t encode_load_store_pair(ASTNode* node);
    uint32_t encode_branch(ASTNode* node);
    uint32_t encode_branch_cond(ASTNode* node, int cond);
    uint32_t encode_cbz_cbnz(ASTNode* node, bool is_cbnz);
    uint32_t encode_system(ASTNode* node);
    uint32_t encode_mov(ASTNode* node);
    
    // Operand extraction helpers
    int get_register(ASTNode* operand, bool* is_64bit);
    int64_t get_immediate(ASTNode* operand);
    bool get_memory_operand(ASTNode* operand, int* base, int64_t* offset, 
                           bool* pre_index, bool* post_index);
    int64_t resolve_label(ASTNode* operand);
    
    // Instruction helpers
    bool token_equals(const Token& token, const char* str) const;
    int get_condition_code(const char* cond, size_t len) const;
    int get_shift_type(const char* shift, size_t len) const;
};

} // namespace casm

#endif // EMBEROS_CASM_CODEGEN_H
