/*
 * EmberOS CASM Parser Header
 * Parser for C.ASM assembly language
 * 
 * Requirements: 7.1, 8.6
 */

#ifndef EMBEROS_CASM_PARSER_H
#define EMBEROS_CASM_PARSER_H

#include "casm/lexer.h"

namespace casm {

/*
 * AST Node types for C.ASM programs
 * Requirements: 8.6
 */
enum class NodeType {
    PROGRAM,        // Root node containing all statements
    LABEL,          // Label definition (name:)
    INSTRUCTION,    // CPU instruction (mnemonic + operands)
    DIRECTIVE,      // Assembler directive (.text, .data, etc.)
    OPERAND_REG,    // Register operand (x0, w1, sp, etc.)
    OPERAND_IMM,    // Immediate operand (#value)
    OPERAND_LABEL,  // Label reference operand
    OPERAND_MEM     // Memory operand ([base, offset])
};

/*
 * Maximum number of children per AST node
 */
constexpr int MAX_AST_CHILDREN = 4;

/*
 * Maximum number of statements in a program
 */
constexpr int MAX_STATEMENTS = 256;

/*
 * Maximum number of nodes in AST pool
 */
constexpr int MAX_AST_NODES = 512;

/*
 * Maximum error message length
 */
constexpr int MAX_ERROR_LEN = 256;

/*
 * Memory addressing mode for OPERAND_MEM nodes
 * Requirements: 8.5
 */
struct MemOperand {
    int base_reg;       // Base register number (0-31, SP=31)
    int index_reg;      // Index register (-1 if none)
    int64_t offset;     // Immediate offset
    bool pre_index;     // Pre-index mode [Xn, #imm]!
    bool post_index;    // Post-index mode [Xn], #imm
    bool is_64bit;      // True for X registers, false for W
};

/*
 * AST Node structure
 * Requirements: 8.6
 */
struct ASTNode {
    NodeType type;
    Token token;                        // Associated token
    ASTNode* children[MAX_AST_CHILDREN]; // Child nodes
    int child_count;                    // Number of children
    
    // For PROGRAM nodes: array of statement pointers
    ASTNode** statements;               // Points to statement array
    int statement_count;                // Number of statements
    
    // Additional data based on node type
    union {
        int reg_num;                    // For OPERAND_REG: register number
        int64_t imm_value;              // For OPERAND_IMM: immediate value
        MemOperand mem;                 // For OPERAND_MEM: memory operand
    } data;
    
    bool is_64bit;                      // For registers: true=X, false=W
};

/*
 * Parser class for C.ASM source code
 * 
 * Usage:
 *   Lexer lexer(source_code);
 *   Parser parser(lexer);
 *   ASTNode* ast = parser.parse();
 *   if (parser.has_error()) {
 *       // handle error
 *   }
 * 
 * Requirements: 7.1, 8.6
 */
class Parser {
public:
    /*
     * Construct a parser with the given lexer
     */
    explicit Parser(Lexer& lexer);
    
    /*
     * Parse the entire source and return AST root
     * Returns nullptr on error
     */
    ASTNode* parse();
    
    /*
     * Check if parsing encountered an error
     */
    bool has_error() const { return has_error_; }
    
    /*
     * Get error message if parsing failed
     */
    const char* get_error() const { return error_msg_; }
    
    /*
     * Get error line number
     */
    int get_error_line() const { return error_line_; }
    
    /*
     * Reset parser state for reuse
     */
    void reset();

private:
    Lexer& lexer_;
    Token current_;
    Token previous_;
    bool has_error_;
    char error_msg_[MAX_ERROR_LEN];
    int error_line_;
    
    // AST node pool (simple allocation)
    ASTNode node_pool_[MAX_AST_NODES];
    int node_count_;
    
    // Statement array for PROGRAM node
    ASTNode* statements_[MAX_STATEMENTS];
    int statement_count_;
    
    // Token handling
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    void consume(TokenType type, const char* message);
    void skip_newlines();
    
    // Error handling
    void error(const char* message);
    void error_at_current(const char* message);
    
    // AST node creation
    ASTNode* alloc_node(NodeType type);
    ASTNode* create_node(NodeType type, const Token& token);
    
    // Parsing methods
    ASTNode* parse_program();
    ASTNode* parse_statement();
    ASTNode* parse_label();
    ASTNode* parse_instruction();
    ASTNode* parse_instruction_with_token(const Token& mnemonic);
    ASTNode* parse_directive();
    ASTNode* parse_operand();
    ASTNode* parse_register();
    ASTNode* parse_immediate();
    ASTNode* parse_memory_operand();
    ASTNode* parse_label_operand();
    
    // Helper methods
    int parse_register_number(const Token& token, bool* is_64bit);
    bool is_instruction_mnemonic(const Token& token) const;
    bool is_directive_name(const Token& token) const;
    bool token_equals(const Token& token, const char* str) const;
};

/*
 * Get string representation of node type
 * Useful for debugging
 */
const char* node_type_to_string(NodeType type);

/*
 * Print AST for debugging
 */
void print_ast(ASTNode* node, int indent = 0);

} // namespace casm

#endif // EMBEROS_CASM_PARSER_H
