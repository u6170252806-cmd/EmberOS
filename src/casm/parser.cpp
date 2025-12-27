/*
 * EmberOS CASM Parser Implementation
 * Parser for C.ASM assembly language
 * 
 * Requirements: 7.1, 8.6
 */

#include "casm/parser.h"

namespace casm {

// String helper functions (freestanding environment)
static size_t str_len(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') len++;
    return len;
}

static void str_copy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static bool str_equal_nocase(const char* a, size_t a_len, const char* b) {
    size_t b_len = str_len(b);
    if (a_len != b_len) return false;
    for (size_t i = 0; i < a_len; i++) {
        if (to_lower(a[i]) != to_lower(b[i])) return false;
    }
    return true;
}

/*
 * ARM64 instruction mnemonics
 * Requirements: 7.2, 7.3, 7.4, 7.5
 */
static const char* INSTRUCTION_MNEMONICS[] = {
    // Data processing
    "add", "adds", "sub", "subs", "adc", "adcs", "sbc", "sbcs",
    "and", "ands", "orr", "eor", "bic", "bics", "orn", "eon",
    "mov", "movz", "movn", "movk", "mvn",
    "asr", "lsl", "lsr", "ror",
    "cmp", "cmn", "tst",
    "neg", "negs", "ngc", "ngcs",
    "mul", "madd", "msub", "mneg",
    "udiv", "sdiv",
    // Load/Store
    "ldr", "ldrb", "ldrh", "ldrsb", "ldrsh", "ldrsw",
    "str", "strb", "strh",
    "ldp", "stp",
    "ldar", "stlr", "ldaxr", "stlxr",
    // Branch
    "b", "bl", "br", "blr", "ret",
    "cbz", "cbnz", "tbz", "tbnz",
    "b.eq", "b.ne", "b.cs", "b.cc", "b.mi", "b.pl",
    "b.vs", "b.vc", "b.hi", "b.ls", "b.ge", "b.lt", "b.gt", "b.le", "b.al",
    "beq", "bne", "bcs", "bcc", "bmi", "bpl",
    "bvs", "bvc", "bhi", "bls", "bge", "blt", "bgt", "ble", "bal",
    // System
    "nop", "wfi", "wfe", "sev", "sevl",
    "svc", "hvc", "smc",
    "mrs", "msr",
    "dmb", "dsb", "isb",
    // SIMD (basic)
    "fmov", "fadd", "fsub", "fmul", "fdiv",
    // CASM Extended I/O opcodes
    "prt", "prtc", "prtn", "prtx", "inp", "inps",
    // CASM Extended Graphics opcodes
    "cls", "setc", "plot", "line", "box", "reset", "canvas",
    // CASM Extended File opcodes
    "fcreat", "fwrite", "fread", "fdel", "fcopy", "fmove", "fexist",
    // CASM Extended System opcodes
    "sleep", "rnd", "tick", "halt",
    // CASM Extended Memory/String opcodes
    "strlen", "memcpy", "memset", "abs",
    nullptr
};

/*
 * Assembler directive names
 * Requirements: 7.8
 */
static const char* DIRECTIVE_NAMES[] = {
    "text", "data", "bss", "section",
    "global", "globl", "extern",
    "align", "balign", "p2align",
    "byte", "hword", "word", "quad", "octa",
    "ascii", "asciz", "string",
    "space", "skip", "fill",
    "equ", "set",
    "include",
    nullptr
};

/*
 * Constructor
 */
Parser::Parser(Lexer& lexer)
    : lexer_(lexer)
    , has_error_(false)
    , error_line_(0)
    , node_count_(0)
    , statement_count_(0)
{
    error_msg_[0] = '\0';
    current_.type = TokenType::END_OF_FILE;
    previous_.type = TokenType::END_OF_FILE;
}

/*
 * Reset parser state
 */
void Parser::reset() {
    has_error_ = false;
    error_msg_[0] = '\0';
    error_line_ = 0;
    node_count_ = 0;
    statement_count_ = 0;
}

/*
 * Advance to next token
 */
void Parser::advance() {
    previous_ = current_;
    current_ = lexer_.next_token();
    
    if (current_.type == TokenType::ERROR) {
        error_at_current(current_.start);
    }
}

/*
 * Check if current token is of given type
 */
bool Parser::check(TokenType type) const {
    return current_.type == type;
}

/*
 * Match and consume token if it matches
 */
bool Parser::match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/*
 * Consume expected token or report error
 */
void Parser::consume(TokenType type, const char* message) {
    if (current_.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

/*
 * Skip newline tokens
 */
void Parser::skip_newlines() {
    while (current_.type == TokenType::NEWLINE) {
        advance();
    }
}

/*
 * Report error at previous token
 */
void Parser::error(const char* message) {
    if (has_error_) return; // Only report first error
    has_error_ = true;
    error_line_ = previous_.line;
    str_copy(error_msg_, message, MAX_ERROR_LEN);
}

/*
 * Report error at current token
 */
void Parser::error_at_current(const char* message) {
    if (has_error_) return;
    has_error_ = true;
    error_line_ = current_.line;
    str_copy(error_msg_, message, MAX_ERROR_LEN);
}

/*
 * Allocate a new AST node from pool
 */
ASTNode* Parser::alloc_node(NodeType type) {
    if (node_count_ >= MAX_AST_NODES) {
        error("Too many AST nodes");
        return nullptr;
    }
    
    ASTNode* node = &node_pool_[node_count_++];
    node->type = type;
    node->child_count = 0;
    node->statement_count = 0;
    node->statements = nullptr;
    node->is_64bit = true;
    for (int i = 0; i < MAX_AST_CHILDREN; i++) {
        node->children[i] = nullptr;
    }
    node->data.imm_value = 0;
    return node;
}

/*
 * Create a new AST node with token
 */
ASTNode* Parser::create_node(NodeType type, const Token& token) {
    ASTNode* node = alloc_node(type);
    if (node) {
        node->token = token;
    }
    return node;
}

/*
 * Check if token matches a string (case-insensitive)
 */
bool Parser::token_equals(const Token& token, const char* str) const {
    return str_equal_nocase(token.start, token.length, str);
}

/*
 * Check if token is an instruction mnemonic
 * Requirements: 7.2, 7.3, 7.4, 7.5
 */
bool Parser::is_instruction_mnemonic(const Token& token) const {
    if (token.type != TokenType::IDENTIFIER) return false;
    
    for (int i = 0; INSTRUCTION_MNEMONICS[i] != nullptr; i++) {
        if (token_equals(token, INSTRUCTION_MNEMONICS[i])) {
            return true;
        }
    }
    return false;
}

/*
 * Check if token is a directive name (without dot)
 * Requirements: 7.8
 */
bool Parser::is_directive_name(const Token& token) const {
    if (token.type != TokenType::IDENTIFIER) return false;
    
    for (int i = 0; DIRECTIVE_NAMES[i] != nullptr; i++) {
        if (token_equals(token, DIRECTIVE_NAMES[i])) {
            return true;
        }
    }
    return false;
}


/*
 * Parse register number from token
 * Returns register number (0-31) or -1 on error
 * Sets is_64bit to true for X registers, false for W
 * Requirements: 8.3
 */
int Parser::parse_register_number(const Token& token, bool* is_64bit) {
    if (token.type != TokenType::IDENTIFIER) return -1;
    if (token.length < 2) return -1;
    
    const char* name = token.start;
    size_t len = token.length;
    
    // Check for sp (stack pointer = x31)
    if (len == 2 && to_lower(name[0]) == 's' && to_lower(name[1]) == 'p') {
        *is_64bit = true;
        return 31;
    }
    
    // Check for lr (link register = x30)
    if (len == 2 && to_lower(name[0]) == 'l' && to_lower(name[1]) == 'r') {
        *is_64bit = true;
        return 30;
    }
    
    // Check for xzr (zero register)
    if (len == 3 && to_lower(name[0]) == 'x' && 
        to_lower(name[1]) == 'z' && to_lower(name[2]) == 'r') {
        *is_64bit = true;
        return 31; // XZR is encoded as register 31
    }
    
    // Check for wzr (32-bit zero register)
    if (len == 3 && to_lower(name[0]) == 'w' && 
        to_lower(name[1]) == 'z' && to_lower(name[2]) == 'r') {
        *is_64bit = false;
        return 31; // WZR is encoded as register 31
    }
    
    // Check for x0-x30 or w0-w30
    char prefix = to_lower(name[0]);
    if (prefix != 'x' && prefix != 'w') return -1;
    
    *is_64bit = (prefix == 'x');
    
    // Parse register number
    int num = 0;
    for (size_t i = 1; i < len; i++) {
        if (name[i] < '0' || name[i] > '9') return -1;
        num = num * 10 + (name[i] - '0');
    }
    
    if (num < 0 || num > 30) return -1;
    return num;
}

/*
 * Parse entire program
 * Requirements: 7.1, 8.6
 */
ASTNode* Parser::parse() {
    advance(); // Get first token
    return parse_program();
}

/*
 * Parse program (list of statements)
 */
ASTNode* Parser::parse_program() {
    ASTNode* program = alloc_node(NodeType::PROGRAM);
    if (!program) return nullptr;
    
    // Use the statement array from parser
    statement_count_ = 0;
    
    skip_newlines();
    
    while (!check(TokenType::END_OF_FILE) && !has_error_) {
        ASTNode* stmt = parse_statement();
        if (stmt) {
            if (statement_count_ < MAX_STATEMENTS) {
                statements_[statement_count_++] = stmt;
            } else {
                error("Too many statements");
                break;
            }
        }
        
        // Expect newline or EOF after statement
        if (!check(TokenType::END_OF_FILE) && !check(TokenType::NEWLINE)) {
            error_at_current("Expected newline after statement");
            return program;
        }
        skip_newlines();
    }
    
    // Link statements to program node
    program->statements = statements_;
    program->statement_count = statement_count_;
    
    return program;
}

/*
 * Parse a single statement (label, instruction, or directive)
 */
ASTNode* Parser::parse_statement() {
    // Check for directive (starts with .)
    if (check(TokenType::DOT)) {
        return parse_directive();
    }
    
    // Must be identifier (label or instruction)
    if (!check(TokenType::IDENTIFIER)) {
        error_at_current("Expected label, instruction, or directive");
        return nullptr;
    }
    
    // Peek ahead to see if this is a label (followed by :)
    Token id_token = current_;
    advance();
    
    if (check(TokenType::COLON)) {
        // This is a label definition
        advance(); // consume ':'
        ASTNode* label = create_node(NodeType::LABEL, id_token);
        return label;
    }
    
    // This is an instruction
    // Put the token back by creating instruction with saved token
    return parse_instruction_with_token(id_token);
}

/*
 * Parse label definition
 */
ASTNode* Parser::parse_label() {
    Token name = current_;
    advance();
    consume(TokenType::COLON, "Expected ':' after label name");
    return create_node(NodeType::LABEL, name);
}




/*
 * Parse instruction (internal helper with pre-consumed token)
 */
ASTNode* Parser::parse_instruction_with_token(const Token& mnemonic) {
    ASTNode* instr = create_node(NodeType::INSTRUCTION, mnemonic);
    if (!instr) return nullptr;
    
    // Parse operands (comma-separated)
    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE) && !has_error_) {
        ASTNode* operand = parse_operand();
        if (!operand) break;
        
        if (instr->child_count < MAX_AST_CHILDREN) {
            instr->children[instr->child_count++] = operand;
        } else {
            error("Too many operands");
            break;
        }
        
        // Check for comma (more operands) or end
        if (!match(TokenType::COMMA)) {
            break;
        }
    }
    
    return instr;
}

/*
 * Parse instruction
 * Requirements: 7.2, 7.3, 7.4, 7.5
 */
ASTNode* Parser::parse_instruction() {
    Token mnemonic = current_;
    advance();
    return parse_instruction_with_token(mnemonic);
}

/*
 * Parse directive
 * Requirements: 7.8
 */
ASTNode* Parser::parse_directive() {
    consume(TokenType::DOT, "Expected '.' for directive");
    
    if (!check(TokenType::IDENTIFIER)) {
        error_at_current("Expected directive name after '.'");
        return nullptr;
    }
    
    Token name = current_;
    advance();
    
    ASTNode* directive = create_node(NodeType::DIRECTIVE, name);
    if (!directive) return nullptr;
    
    // Parse directive arguments
    while (!check(TokenType::NEWLINE) && !check(TokenType::END_OF_FILE) && !has_error_) {
        // Directives can have various argument types
        if (check(TokenType::IDENTIFIER)) {
            // Could be a symbol reference
            ASTNode* arg = create_node(NodeType::OPERAND_LABEL, current_);
            advance();
            if (arg && directive->child_count < MAX_AST_CHILDREN) {
                directive->children[directive->child_count++] = arg;
            }
        } else if (check(TokenType::NUMBER)) {
            ASTNode* arg = alloc_node(NodeType::OPERAND_IMM);
            if (arg) {
                arg->token = current_;
                arg->data.imm_value = current_.number_value;
                if (directive->child_count < MAX_AST_CHILDREN) {
                    directive->children[directive->child_count++] = arg;
                }
            }
            advance();
        } else if (check(TokenType::STRING)) {
            // String argument for .ascii, .asciz, etc.
            ASTNode* arg = create_node(NodeType::OPERAND_LABEL, current_);
            advance();
            if (arg && directive->child_count < MAX_AST_CHILDREN) {
                directive->children[directive->child_count++] = arg;
            }
        } else if (check(TokenType::HASH)) {
            // Immediate value
            ASTNode* arg = parse_immediate();
            if (arg && directive->child_count < MAX_AST_CHILDREN) {
                directive->children[directive->child_count++] = arg;
            }
        } else {
            break;
        }
        
        // Check for comma
        if (!match(TokenType::COMMA)) {
            break;
        }
    }
    
    return directive;
}

/*
 * Parse operand (register, immediate, memory, or label)
 * Requirements: 8.3, 8.4, 8.5
 */
ASTNode* Parser::parse_operand() {
    // Memory operand [...]
    if (check(TokenType::LBRACKET)) {
        return parse_memory_operand();
    }
    
    // Immediate #value
    if (check(TokenType::HASH)) {
        return parse_immediate();
    }
    
    // Register or label reference
    if (check(TokenType::IDENTIFIER)) {
        bool is_64bit;
        int reg_num = parse_register_number(current_, &is_64bit);
        
        if (reg_num >= 0) {
            return parse_register();
        } else {
            // Label reference
            return parse_label_operand();
        }
    }
    
    // Negative immediate without #
    if (check(TokenType::MINUS)) {
        advance();
        if (check(TokenType::NUMBER)) {
            ASTNode* node = alloc_node(NodeType::OPERAND_IMM);
            if (node) {
                node->token = current_;
                node->data.imm_value = -current_.number_value;
            }
            advance();
            return node;
        }
        error_at_current("Expected number after '-'");
        return nullptr;
    }
    
    // Plain number (some assemblers allow this)
    if (check(TokenType::NUMBER)) {
        ASTNode* node = alloc_node(NodeType::OPERAND_IMM);
        if (node) {
            node->token = current_;
            node->data.imm_value = current_.number_value;
        }
        advance();
        return node;
    }
    
    error_at_current("Expected operand");
    return nullptr;
}

/*
 * Parse register operand
 * Requirements: 8.3
 */
ASTNode* Parser::parse_register() {
    bool is_64bit;
    int reg_num = parse_register_number(current_, &is_64bit);
    
    if (reg_num < 0) {
        error_at_current("Invalid register name");
        return nullptr;
    }
    
    ASTNode* node = create_node(NodeType::OPERAND_REG, current_);
    if (node) {
        node->data.reg_num = reg_num;
        node->is_64bit = is_64bit;
    }
    advance();
    return node;
}

/*
 * Parse immediate operand (#value)
 * Requirements: 8.4
 */
ASTNode* Parser::parse_immediate() {
    consume(TokenType::HASH, "Expected '#' for immediate");
    
    bool negative = false;
    if (match(TokenType::MINUS)) {
        negative = true;
    }
    
    if (!check(TokenType::NUMBER) && !check(TokenType::IDENTIFIER)) {
        error_at_current("Expected number or symbol after '#'");
        return nullptr;
    }
    
    ASTNode* node = alloc_node(NodeType::OPERAND_IMM);
    if (!node) return nullptr;
    
    node->token = current_;
    
    if (check(TokenType::NUMBER)) {
        node->data.imm_value = negative ? -current_.number_value : current_.number_value;
        advance();
    } else {
        // Symbol reference as immediate (will be resolved later)
        node->type = NodeType::OPERAND_LABEL;
        advance();
    }
    
    return node;
}


/*
 * Parse memory operand [base, offset] or [base], offset
 * Requirements: 8.5
 */
ASTNode* Parser::parse_memory_operand() {
    consume(TokenType::LBRACKET, "Expected '[' for memory operand");
    
    ASTNode* node = alloc_node(NodeType::OPERAND_MEM);
    if (!node) return nullptr;
    
    // Initialize memory operand
    node->data.mem.base_reg = -1;
    node->data.mem.index_reg = -1;
    node->data.mem.offset = 0;
    node->data.mem.pre_index = false;
    node->data.mem.post_index = false;
    node->data.mem.is_64bit = true;
    
    // Parse base register
    if (!check(TokenType::IDENTIFIER)) {
        error_at_current("Expected base register in memory operand");
        return nullptr;
    }
    
    bool is_64bit;
    int base_reg = parse_register_number(current_, &is_64bit);
    if (base_reg < 0) {
        error_at_current("Invalid base register");
        return nullptr;
    }
    
    node->token = current_;
    node->data.mem.base_reg = base_reg;
    node->data.mem.is_64bit = is_64bit;
    advance();
    
    // Check for offset or index register
    if (match(TokenType::COMMA)) {
        // Could be immediate offset or index register
        if (check(TokenType::HASH)) {
            // Immediate offset: [base, #imm]
            advance(); // consume #
            
            bool negative = false;
            if (match(TokenType::MINUS)) {
                negative = true;
            }
            
            if (!check(TokenType::NUMBER)) {
                error_at_current("Expected number after '#' in memory operand");
                return nullptr;
            }
            
            node->data.mem.offset = negative ? -current_.number_value : current_.number_value;
            advance();
        } else if (check(TokenType::IDENTIFIER)) {
            // Index register: [base, Xm] or [base, Xm, LSL #n]
            bool idx_64bit;
            int idx_reg = parse_register_number(current_, &idx_64bit);
            if (idx_reg >= 0) {
                node->data.mem.index_reg = idx_reg;
                advance();
                
                // Check for shift/extend
                // TODO: Handle LSL, SXTW, etc.
            } else {
                error_at_current("Invalid index register");
                return nullptr;
            }
        } else if (check(TokenType::NUMBER)) {
            // Plain number offset (some assemblers)
            node->data.mem.offset = current_.number_value;
            advance();
        } else if (check(TokenType::MINUS)) {
            // Negative offset without #
            advance();
            if (!check(TokenType::NUMBER)) {
                error_at_current("Expected number after '-'");
                return nullptr;
            }
            node->data.mem.offset = -current_.number_value;
            advance();
        }
    }
    
    consume(TokenType::RBRACKET, "Expected ']' after memory operand");
    
    // Check for pre-index (!)
    if (match(TokenType::EXCLAIM)) {
        node->data.mem.pre_index = true;
    }
    
    // Check for post-index (, #imm after ])
    if (match(TokenType::COMMA)) {
        node->data.mem.post_index = true;
        
        if (match(TokenType::HASH)) {
            bool negative = false;
            if (match(TokenType::MINUS)) {
                negative = true;
            }
            
            if (!check(TokenType::NUMBER)) {
                error_at_current("Expected number for post-index offset");
                return nullptr;
            }
            
            node->data.mem.offset = negative ? -current_.number_value : current_.number_value;
            advance();
        } else if (check(TokenType::NUMBER)) {
            node->data.mem.offset = current_.number_value;
            advance();
        } else if (check(TokenType::MINUS)) {
            advance();
            if (!check(TokenType::NUMBER)) {
                error_at_current("Expected number after '-'");
                return nullptr;
            }
            node->data.mem.offset = -current_.number_value;
            advance();
        }
    }
    
    return node;
}

/*
 * Parse label reference operand
 */
ASTNode* Parser::parse_label_operand() {
    ASTNode* node = create_node(NodeType::OPERAND_LABEL, current_);
    advance();
    return node;
}

/*
 * Get string representation of node type
 */
const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NodeType::PROGRAM:      return "PROGRAM";
        case NodeType::LABEL:        return "LABEL";
        case NodeType::INSTRUCTION:  return "INSTRUCTION";
        case NodeType::DIRECTIVE:    return "DIRECTIVE";
        case NodeType::OPERAND_REG:  return "OPERAND_REG";
        case NodeType::OPERAND_IMM:  return "OPERAND_IMM";
        case NodeType::OPERAND_LABEL: return "OPERAND_LABEL";
        case NodeType::OPERAND_MEM:  return "OPERAND_MEM";
        default:                     return "UNKNOWN";
    }
}

/*
 * Print AST for debugging (requires uart::printf)
 */
void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    
    // This would use uart::printf in the actual kernel
    // For now, just a placeholder
    (void)node;
    (void)indent;
}

} // namespace casm
