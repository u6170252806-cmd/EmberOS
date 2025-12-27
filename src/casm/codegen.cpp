/*
 * EmberOS CASM Code Generator Implementation
 * Generates ARM64 machine code from AST
 * 
 * Requirements: 7.2, 7.3, 7.4, 7.5, 7.6, 7.8, 7.9, 7.10
 */

#include "casm/codegen.h"

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

static void str_copy_n(char* dest, const char* src, size_t n, size_t max_len) {
    size_t i = 0;
    size_t limit = (n < max_len - 1) ? n : max_len - 1;
    while (i < limit && src[i] != '\0') {
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

static bool str_equal_n(const char* a, size_t a_len, const char* b, size_t b_len) {
    if (a_len != b_len) return false;
    for (size_t i = 0; i < a_len; i++) {
        if (to_lower(a[i]) != to_lower(b[i])) return false;
    }
    return true;
}

/*
 * Constructor
 */
CodeGenerator::CodeGenerator()
    : code_offset_(0)
    , symbol_count_(0)
    , current_section_(Section::TEXT)
    , has_error_(false)
    , error_line_(0)
    , current_line_(1)
    , is_first_pass_(true)
{
    error_msg_[0] = '\0';
    // Zero out code buffer
    for (size_t i = 0; i < MAX_CODE_SIZE; i++) {
        code_[i] = 0;
    }
}

/*
 * Reset generator state
 */
void CodeGenerator::reset() {
    code_offset_ = 0;
    symbol_count_ = 0;
    current_section_ = Section::TEXT;
    has_error_ = false;
    error_msg_[0] = '\0';
    error_line_ = 0;
    current_line_ = 1;
    is_first_pass_ = true;
}

/*
 * Report error
 * Requirements: 7.9
 */
void CodeGenerator::error(const char* message) {
    if (has_error_) return;
    has_error_ = true;
    error_line_ = current_line_;
    str_copy(error_msg_, message, MAX_CODEGEN_ERROR);
}

/*
 * Report error at specific line
 * Requirements: 7.9
 */
void CodeGenerator::error_at(int line, const char* message) {
    if (has_error_) return;
    has_error_ = true;
    error_line_ = line;
    str_copy(error_msg_, message, MAX_CODEGEN_ERROR);
}

/*
 * Check if token matches string (case-insensitive)
 */
bool CodeGenerator::token_equals(const Token& token, const char* str) const {
    return str_equal_nocase(token.start, token.length, str);
}

/*
 * Add a new symbol to the table
 * Requirements: 7.6
 */
Symbol* CodeGenerator::add_symbol(const char* name, size_t len) {
    if (symbol_count_ >= MAX_SYMBOLS) {
        error("Symbol table full");
        return nullptr;
    }
    
    Symbol* sym = &symbols_[symbol_count_++];
    str_copy_n(sym->name, name, len, MAX_SYMBOL_NAME);
    sym->address = 0;
    sym->defined = false;
    sym->is_global = false;
    return sym;
}

/*
 * Find a symbol by name
 * Requirements: 7.6
 */
Symbol* CodeGenerator::find_symbol(const char* name, size_t len) {
    for (int i = 0; i < symbol_count_; i++) {
        size_t sym_len = str_len(symbols_[i].name);
        if (str_equal_n(name, len, symbols_[i].name, sym_len)) {
            return &symbols_[i];
        }
    }
    return nullptr;
}

/*
 * Look up a symbol (const version)
 */
const Symbol* CodeGenerator::lookup_symbol(const char* name, size_t len) const {
    for (int i = 0; i < symbol_count_; i++) {
        size_t sym_len = str_len(symbols_[i].name);
        if (str_equal_n(name, len, symbols_[i].name, sym_len)) {
            return &symbols_[i];
        }
    }
    return nullptr;
}

/*
 * Get symbol by index
 */
const Symbol* CodeGenerator::get_symbol(int index) const {
    if (index < 0 || index >= symbol_count_) return nullptr;
    return &symbols_[index];
}

/*
 * Define a symbol with address
 * Requirements: 7.6
 */
bool CodeGenerator::define_symbol(const char* name, size_t len, uint64_t address) {
    Symbol* sym = find_symbol(name, len);
    
    if (sym) {
        if (sym->defined) {
            error("Symbol already defined");
            return false;
        }
        sym->address = address;
        sym->defined = true;
        return true;
    }
    
    sym = add_symbol(name, len);
    if (!sym) return false;
    
    sym->address = address;
    sym->defined = true;
    return true;
}

/*
 * Emit a single byte
 * Requirements: 7.10
 */
void CodeGenerator::emit_byte(uint8_t byte) {
    if (is_first_pass_) {
        code_offset_++;
        return;
    }
    
    if (code_offset_ >= MAX_CODE_SIZE) {
        error("Code buffer overflow");
        return;
    }
    code_[code_offset_++] = byte;
}

/*
 * Emit a 16-bit halfword (little-endian)
 */
void CodeGenerator::emit_half(uint16_t half) {
    emit_byte(half & 0xFF);
    emit_byte((half >> 8) & 0xFF);
}

/*
 * Emit a 32-bit word (little-endian)
 */
void CodeGenerator::emit_word(uint32_t word) {
    emit_byte(word & 0xFF);
    emit_byte((word >> 8) & 0xFF);
    emit_byte((word >> 16) & 0xFF);
    emit_byte((word >> 24) & 0xFF);
}

/*
 * Emit a 64-bit quad (little-endian)
 */
void CodeGenerator::emit_quad(uint64_t quad) {
    emit_word(quad & 0xFFFFFFFF);
    emit_word((quad >> 32) & 0xFFFFFFFF);
}

/*
 * Align code offset to boundary
 * Requirements: 7.8
 */
void CodeGenerator::align_to(size_t alignment) {
    while (code_offset_ % alignment != 0) {
        emit_byte(0);
    }
}


/*
 * Generate machine code from AST
 * Implements two-pass assembly
 * Requirements: 7.6
 */
bool CodeGenerator::generate(ASTNode* ast) {
    if (!ast) {
        error("Null AST");
        return false;
    }
    
    // Pass 1: Collect labels and calculate addresses
    is_first_pass_ = true;
    code_offset_ = 0;
    if (!first_pass(ast)) {
        return false;
    }
    
    // Pass 2: Generate code with resolved labels
    is_first_pass_ = false;
    code_offset_ = 0;
    current_line_ = 1;
    if (!second_pass(ast)) {
        return false;
    }
    
    return !has_error_;
}

/*
 * First pass: collect labels and calculate addresses
 * Requirements: 7.6
 */
bool CodeGenerator::first_pass(ASTNode* ast) {
    if (!ast) return true;
    
    if (ast->type == NodeType::PROGRAM) {
        // Use statement array instead of children
        for (int i = 0; i < ast->statement_count && !has_error_; i++) {
            first_pass(ast->statements[i]);
        }
    } else {
        process_node(ast);
    }
    
    return !has_error_;
}

/*
 * Second pass: generate machine code
 */
bool CodeGenerator::second_pass(ASTNode* ast) {
    if (!ast) return true;
    
    if (ast->type == NodeType::PROGRAM) {
        // Use statement array instead of children
        for (int i = 0; i < ast->statement_count && !has_error_; i++) {
            second_pass(ast->statements[i]);
        }
    } else {
        process_node(ast);
    }
    
    return !has_error_;
}

/*
 * Process a single AST node
 */
void CodeGenerator::process_node(ASTNode* node) {
    if (!node || has_error_) return;
    
    current_line_ = node->token.line;
    
    switch (node->type) {
        case NodeType::LABEL:
            process_label(node);
            break;
        case NodeType::INSTRUCTION:
            process_instruction(node);
            break;
        case NodeType::DIRECTIVE:
            process_directive(node);
            break;
        default:
            break;
    }
}

/*
 * Process label definition
 * Requirements: 7.6
 */
void CodeGenerator::process_label(ASTNode* node) {
    if (is_first_pass_) {
        // Define the label with current address
        if (!define_symbol(node->token.start, node->token.length, code_offset_)) {
            // Error already set
        }
    }
    // Labels don't emit code
}

/*
 * Process directive
 * Requirements: 7.8
 */
void CodeGenerator::process_directive(ASTNode* node) {
    const Token& name = node->token;
    
    // .text - switch to text section
    if (token_equals(name, "text")) {
        current_section_ = Section::TEXT;
        return;
    }
    
    // .data - switch to data section
    if (token_equals(name, "data")) {
        current_section_ = Section::DATA;
        return;
    }
    
    // .bss - switch to BSS section
    if (token_equals(name, "bss")) {
        current_section_ = Section::BSS;
        return;
    }
    
    // .global / .globl - mark symbol as global
    if (token_equals(name, "global") || token_equals(name, "globl")) {
        if (node->child_count > 0 && node->children[0]) {
            ASTNode* sym_node = node->children[0];
            Symbol* sym = find_symbol(sym_node->token.start, sym_node->token.length);
            if (!sym) {
                sym = add_symbol(sym_node->token.start, sym_node->token.length);
            }
            if (sym) {
                sym->is_global = true;
            }
        }
        return;
    }
    
    // .align - align to power of 2
    if (token_equals(name, "align") || token_equals(name, "p2align")) {
        if (node->child_count > 0 && node->children[0]) {
            int64_t power = node->children[0]->data.imm_value;
            if (power >= 0 && power <= 12) {
                size_t alignment = 1UL << power;
                align_to(alignment);
            }
        }
        return;
    }
    
    // .balign - align to byte boundary
    if (token_equals(name, "balign")) {
        if (node->child_count > 0 && node->children[0]) {
            int64_t alignment = node->children[0]->data.imm_value;
            if (alignment > 0) {
                align_to(alignment);
            }
        }
        return;
    }
    
    // .byte - emit bytes
    if (token_equals(name, "byte")) {
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i]) {
                int64_t val = node->children[i]->data.imm_value;
                emit_byte(val & 0xFF);
            }
        }
        return;
    }
    
    // .hword - emit 16-bit values
    if (token_equals(name, "hword")) {
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i]) {
                int64_t val = node->children[i]->data.imm_value;
                emit_half(val & 0xFFFF);
            }
        }
        return;
    }
    
    // .word - emit 32-bit values
    if (token_equals(name, "word")) {
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i]) {
                int64_t val = node->children[i]->data.imm_value;
                emit_word(val & 0xFFFFFFFF);
            }
        }
        return;
    }
    
    // .quad - emit 64-bit values
    if (token_equals(name, "quad")) {
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i]) {
                int64_t val = node->children[i]->data.imm_value;
                emit_quad(val);
            }
        }
        return;
    }
    
    // .space / .skip - reserve space
    if (token_equals(name, "space") || token_equals(name, "skip")) {
        if (node->child_count > 0 && node->children[0]) {
            int64_t size = node->children[0]->data.imm_value;
            uint8_t fill = 0;
            if (node->child_count > 1 && node->children[1]) {
                fill = node->children[1]->data.imm_value & 0xFF;
            }
            for (int64_t i = 0; i < size; i++) {
                emit_byte(fill);
            }
        }
        return;
    }
    
    // .ascii - emit string without null terminator
    if (token_equals(name, "ascii")) {
        if (node->child_count > 0 && node->children[0]) {
            const Token& str_tok = node->children[0]->token;
            // Skip quotes
            for (size_t i = 1; i < str_tok.length - 1; i++) {
                char c = str_tok.start[i];
                if (c == '\\' && i + 1 < str_tok.length - 1) {
                    i++;
                    switch (str_tok.start[i]) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case '0': c = '\0'; break;
                        case '\\': c = '\\'; break;
                        case '"': c = '"'; break;
                        default: c = str_tok.start[i]; break;
                    }
                }
                emit_byte(c);
            }
        }
        return;
    }
    
    // .asciz / .string - emit string with null terminator
    if (token_equals(name, "asciz") || token_equals(name, "string")) {
        if (node->child_count > 0 && node->children[0]) {
            const Token& str_tok = node->children[0]->token;
            // Skip quotes
            for (size_t i = 1; i < str_tok.length - 1; i++) {
                char c = str_tok.start[i];
                if (c == '\\' && i + 1 < str_tok.length - 1) {
                    i++;
                    switch (str_tok.start[i]) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case '0': c = '\0'; break;
                        case '\\': c = '\\'; break;
                        case '"': c = '"'; break;
                        default: c = str_tok.start[i]; break;
                    }
                }
                emit_byte(c);
            }
            emit_byte(0); // Null terminator
        }
        return;
    }
    
    // .equ / .set - define constant (handled in first pass only)
    if (token_equals(name, "equ") || token_equals(name, "set")) {
        if (is_first_pass_ && node->child_count >= 2) {
            ASTNode* name_node = node->children[0];
            ASTNode* value_node = node->children[1];
            if (name_node && value_node) {
                define_symbol(name_node->token.start, name_node->token.length,
                             value_node->data.imm_value);
            }
        }
        return;
    }
    
    // Unknown directive - ignore for now
}


/*
 * Get register number from operand
 */
int CodeGenerator::get_register(ASTNode* operand, bool* is_64bit) {
    if (!operand) return -1;
    
    if (operand->type == NodeType::OPERAND_REG) {
        if (is_64bit) *is_64bit = operand->is_64bit;
        return operand->data.reg_num;
    }
    
    return -1;
}

/*
 * Get immediate value from operand
 */
int64_t CodeGenerator::get_immediate(ASTNode* operand) {
    if (!operand) return 0;
    
    if (operand->type == NodeType::OPERAND_IMM) {
        return operand->data.imm_value;
    }
    
    if (operand->type == NodeType::OPERAND_LABEL) {
        return resolve_label(operand);
    }
    
    return 0;
}

/*
 * Get memory operand details
 */
bool CodeGenerator::get_memory_operand(ASTNode* operand, int* base, int64_t* offset,
                                       bool* pre_index, bool* post_index) {
    if (!operand || operand->type != NodeType::OPERAND_MEM) return false;
    
    *base = operand->data.mem.base_reg;
    *offset = operand->data.mem.offset;
    *pre_index = operand->data.mem.pre_index;
    *post_index = operand->data.mem.post_index;
    
    return true;
}

/*
 * Resolve label to address
 * Requirements: 7.6
 */
int64_t CodeGenerator::resolve_label(ASTNode* operand) {
    if (!operand) return 0;
    
    const Symbol* sym = lookup_symbol(operand->token.start, operand->token.length);
    if (!sym) {
        if (!is_first_pass_) {
            error("Undefined symbol");
        }
        return 0;
    }
    
    if (!sym->defined && !is_first_pass_) {
        error("Undefined symbol");
        return 0;
    }
    
    return sym->address;
}

/*
 * Get condition code from string
 */
int CodeGenerator::get_condition_code(const char* cond, size_t len) const {
    if (len < 2) return -1;
    
    // Standard condition codes
    if (str_equal_nocase(cond, len, "eq")) return 0;   // Equal
    if (str_equal_nocase(cond, len, "ne")) return 1;   // Not equal
    if (str_equal_nocase(cond, len, "cs") || str_equal_nocase(cond, len, "hs")) return 2;   // Carry set / unsigned higher or same
    if (str_equal_nocase(cond, len, "cc") || str_equal_nocase(cond, len, "lo")) return 3;   // Carry clear / unsigned lower
    if (str_equal_nocase(cond, len, "mi")) return 4;   // Minus / negative
    if (str_equal_nocase(cond, len, "pl")) return 5;   // Plus / positive or zero
    if (str_equal_nocase(cond, len, "vs")) return 6;   // Overflow
    if (str_equal_nocase(cond, len, "vc")) return 7;   // No overflow
    if (str_equal_nocase(cond, len, "hi")) return 8;   // Unsigned higher
    if (str_equal_nocase(cond, len, "ls")) return 9;   // Unsigned lower or same
    if (str_equal_nocase(cond, len, "ge")) return 10;  // Signed greater or equal
    if (str_equal_nocase(cond, len, "lt")) return 11;  // Signed less than
    if (str_equal_nocase(cond, len, "gt")) return 12;  // Signed greater than
    if (str_equal_nocase(cond, len, "le")) return 13;  // Signed less or equal
    if (str_equal_nocase(cond, len, "al")) return 14;  // Always
    
    return -1;
}

/*
 * Process instruction
 * Requirements: 7.2, 7.3, 7.4, 7.5
 */
void CodeGenerator::process_instruction(ASTNode* node) {
    uint32_t encoding = encode_instruction(node);
    if (!has_error_) {
        emit_word(encoding);
    }
}

/*
 * Encode instruction to machine code
 * Requirements: 7.2, 7.3, 7.4, 7.5
 */
uint32_t CodeGenerator::encode_instruction(ASTNode* node) {
    if (!node || node->type != NodeType::INSTRUCTION) {
        error("Invalid instruction node");
        return 0;
    }
    
    const Token& mnemonic = node->token;
    
    // NOP - special case
    if (token_equals(mnemonic, "nop")) {
        return 0xD503201F;  // NOP encoding
    }
    
    // RET - special case
    if (token_equals(mnemonic, "ret")) {
        int reg = 30;  // Default to LR (x30)
        if (node->child_count > 0) {
            bool is_64bit;
            reg = get_register(node->children[0], &is_64bit);
            if (reg < 0) reg = 30;
        }
        // RET Xn: 1101011 0010 11111 000000 Rn 00000
        return 0xD65F0000 | (reg << 5);
    }
    
    // BR - branch to register
    if (token_equals(mnemonic, "br")) {
        if (node->child_count < 1) {
            error("BR requires register operand");
            return 0;
        }
        bool is_64bit;
        int reg = get_register(node->children[0], &is_64bit);
        if (reg < 0) {
            error("Invalid register for BR");
            return 0;
        }
        // BR Xn: 1101011 0000 11111 000000 Rn 00000
        return 0xD61F0000 | (reg << 5);
    }
    
    // BLR - branch with link to register
    if (token_equals(mnemonic, "blr")) {
        if (node->child_count < 1) {
            error("BLR requires register operand");
            return 0;
        }
        bool is_64bit;
        int reg = get_register(node->children[0], &is_64bit);
        if (reg < 0) {
            error("Invalid register for BLR");
            return 0;
        }
        // BLR Xn: 1101011 0001 11111 000000 Rn 00000
        return 0xD63F0000 | (reg << 5);
    }
    
    // B - unconditional branch
    if (token_equals(mnemonic, "b")) {
        return encode_branch(node);
    }
    
    // BL - branch with link
    if (token_equals(mnemonic, "bl")) {
        return encode_branch(node);
    }
    
    // Conditional branches (b.eq, b.ne, etc.)
    if (mnemonic.length > 2 && mnemonic.start[0] == 'b' && 
        (mnemonic.start[1] == '.' || 
         (mnemonic.length >= 3 && to_lower(mnemonic.start[1]) != 'l' && to_lower(mnemonic.start[1]) != 'r'))) {
        
        // Check for b.cond format
        if (mnemonic.start[1] == '.') {
            int cond = get_condition_code(mnemonic.start + 2, mnemonic.length - 2);
            if (cond >= 0) {
                return encode_branch_cond(node, cond);
            }
        }
        // Check for bcond format (beq, bne, etc.)
        else {
            int cond = get_condition_code(mnemonic.start + 1, mnemonic.length - 1);
            if (cond >= 0) {
                return encode_branch_cond(node, cond);
            }
        }
    }
    
    // CBZ / CBNZ
    if (token_equals(mnemonic, "cbz")) {
        return encode_cbz_cbnz(node, false);
    }
    if (token_equals(mnemonic, "cbnz")) {
        return encode_cbz_cbnz(node, true);
    }
    
    // MOV variants
    if (token_equals(mnemonic, "mov") || token_equals(mnemonic, "movz") ||
        token_equals(mnemonic, "movn") || token_equals(mnemonic, "movk")) {
        return encode_mov(node);
    }
    
    // Data processing instructions
    if (token_equals(mnemonic, "add") || token_equals(mnemonic, "adds") ||
        token_equals(mnemonic, "sub") || token_equals(mnemonic, "subs") ||
        token_equals(mnemonic, "and") || token_equals(mnemonic, "ands") ||
        token_equals(mnemonic, "orr") || token_equals(mnemonic, "eor") ||
        token_equals(mnemonic, "bic") || token_equals(mnemonic, "orn") ||
        token_equals(mnemonic, "mvn") || token_equals(mnemonic, "neg") ||
        token_equals(mnemonic, "cmp") || token_equals(mnemonic, "cmn") ||
        token_equals(mnemonic, "tst") ||
        token_equals(mnemonic, "asr") || token_equals(mnemonic, "lsl") ||
        token_equals(mnemonic, "lsr") || token_equals(mnemonic, "ror") ||
        token_equals(mnemonic, "mul") || token_equals(mnemonic, "udiv") ||
        token_equals(mnemonic, "sdiv")) {
        return encode_data_proc(node);
    }
    
    // Load/Store instructions
    if (token_equals(mnemonic, "ldr") || token_equals(mnemonic, "ldrb") ||
        token_equals(mnemonic, "ldrh") || token_equals(mnemonic, "ldrsb") ||
        token_equals(mnemonic, "ldrsh") || token_equals(mnemonic, "ldrsw") ||
        token_equals(mnemonic, "str") || token_equals(mnemonic, "strb") ||
        token_equals(mnemonic, "strh")) {
        return encode_load_store(node);
    }
    
    // Load/Store pair
    if (token_equals(mnemonic, "ldp") || token_equals(mnemonic, "stp")) {
        return encode_load_store_pair(node);
    }
    
    // System instructions
    if (token_equals(mnemonic, "wfi") || token_equals(mnemonic, "wfe") ||
        token_equals(mnemonic, "sev") || token_equals(mnemonic, "sevl") ||
        token_equals(mnemonic, "dmb") || token_equals(mnemonic, "dsb") ||
        token_equals(mnemonic, "isb") || token_equals(mnemonic, "svc") ||
        token_equals(mnemonic, "hvc") || token_equals(mnemonic, "smc")) {
        return encode_system(node);
    }
    
    // CASM Extended I/O opcodes (encoded as SVC with special immediates)
    if (token_equals(mnemonic, "prt")) {
        return 0xD4002001;  // SVC #0x100
    }
    if (token_equals(mnemonic, "prtc")) {
        return 0xD4002021;  // SVC #0x101
    }
    if (token_equals(mnemonic, "prtn")) {
        return 0xD4002041;  // SVC #0x102
    }
    if (token_equals(mnemonic, "inp")) {
        return 0xD4002061;  // SVC #0x103
    }
    
    // CASM Extended Graphics opcodes
    if (token_equals(mnemonic, "cls")) {
        return 0xD4002201;  // SVC #0x110
    }
    if (token_equals(mnemonic, "setc")) {
        return 0xD4002221;  // SVC #0x111
    }
    if (token_equals(mnemonic, "plot")) {
        return 0xD4002241;  // SVC #0x112
    }
    if (token_equals(mnemonic, "line")) {
        return 0xD4002261;  // SVC #0x113
    }
    if (token_equals(mnemonic, "box")) {
        return 0xD4002281;  // SVC #0x114
    }
    if (token_equals(mnemonic, "reset")) {
        return 0xD40022A1;  // SVC #0x115
    }
    if (token_equals(mnemonic, "canvas")) {
        return 0xD40022C1;  // SVC #0x116
    }
    
    // CASM Extended System opcodes
    if (token_equals(mnemonic, "sleep")) {
        return 0xD4003E01;  // SVC #0x1F0
    }
    if (token_equals(mnemonic, "rnd")) {
        return 0xD4003E21;  // SVC #0x1F1
    }
    if (token_equals(mnemonic, "tick")) {
        return 0xD4003E41;  // SVC #0x1F2
    }
    if (token_equals(mnemonic, "halt")) {
        return 0xD4003FE1;  // SVC #0x1FF
    }
    
    // CASM Extended I/O opcodes (additional)
    if (token_equals(mnemonic, "inps")) {
        return 0xD4002081;  // SVC #0x104 - input string
    }
    if (token_equals(mnemonic, "prtx")) {
        return 0xD40020A1;  // SVC #0x105 - print hex
    }
    
    // CASM Extended Memory/String opcodes (0x130-0x13F)
    if (token_equals(mnemonic, "strlen")) {
        return 0xD4002601;  // SVC #0x130 - strlen (x0=addr -> x0=len)
    }
    if (token_equals(mnemonic, "memcpy")) {
        return 0xD4002621;  // SVC #0x131 - memcpy (x0=dst, x1=src, x2=len)
    }
    if (token_equals(mnemonic, "memset")) {
        return 0xD4002641;  // SVC #0x132 - memset (x0=addr, x1=byte, x2=len)
    }
    if (token_equals(mnemonic, "abs")) {
        return 0xD4002661;  // SVC #0x133 - abs (x0=val -> x0=|val|)
    }
    
    // CASM Extended File opcodes (0x120-0x12F)
    if (token_equals(mnemonic, "fcreat")) {
        return 0xD4002401;  // SVC #0x120 - create file (x0=name_ptr)
    }
    if (token_equals(mnemonic, "fwrite")) {
        return 0xD4002421;  // SVC #0x121 - write to file (x0=name_ptr, x1=data_ptr, x2=len)
    }
    if (token_equals(mnemonic, "fread")) {
        return 0xD4002441;  // SVC #0x122 - read file (x0=name_ptr, x1=buf_ptr, x2=max_len) -> x0=bytes_read
    }
    if (token_equals(mnemonic, "fdel")) {
        return 0xD4002461;  // SVC #0x123 - delete file (x0=name_ptr)
    }
    if (token_equals(mnemonic, "fcopy")) {
        return 0xD4002481;  // SVC #0x124 - copy file (x0=src_ptr, x1=dst_ptr)
    }
    if (token_equals(mnemonic, "fmove")) {
        return 0xD40024A1;  // SVC #0x125 - move/rename file (x0=src_ptr, x1=dst_ptr)
    }
    if (token_equals(mnemonic, "fexist")) {
        return 0xD40024C1;  // SVC #0x126 - check if file exists (x0=name_ptr) -> x0=1 if exists
    }
    
    error("Unknown instruction");
    return 0;
}


/*
 * Encode data processing instruction
 * Requirements: 7.2
 */
uint32_t CodeGenerator::encode_data_proc(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    // Check if we have enough operands
    if (node->child_count < 2) {
        error("Data processing instruction requires at least 2 operands");
        return 0;
    }
    
    bool is_64bit = true;
    int rd = -1, rn = -1, rm = -1;
    int64_t imm = 0;
    bool has_imm = false;
    (void)imm;  // May be unused in register form
    
    // Get destination register
    rd = get_register(node->children[0], &is_64bit);
    if (rd < 0) {
        error("Invalid destination register");
        return 0;
    }
    
    // Handle CMP, CMN, TST (2 operands, implicit Rd=XZR)
    if (token_equals(mnemonic, "cmp") || token_equals(mnemonic, "cmn") ||
        token_equals(mnemonic, "tst")) {
        rn = rd;
        rd = 31;  // XZR
        
        if (node->children[1]->type == NodeType::OPERAND_IMM ||
            node->children[1]->type == NodeType::OPERAND_LABEL) {
            has_imm = true;
            imm = get_immediate(node->children[1]);
        } else {
            rm = get_register(node->children[1], nullptr);
            if (rm < 0) {
                error("Invalid second operand");
                return 0;
            }
        }
    }
    // Handle NEG, MVN (2 operands)
    else if (token_equals(mnemonic, "neg") || token_equals(mnemonic, "mvn")) {
        rn = 31;  // XZR for NEG/MVN
        rm = get_register(node->children[1], nullptr);
        if (rm < 0) {
            error("Invalid source register");
            return 0;
        }
    }
    // Standard 3-operand instructions
    else {
        if (node->child_count < 3) {
            // 2-operand form: Rd, Rm -> Rd, Rd, Rm
            rn = rd;
            if (node->children[1]->type == NodeType::OPERAND_IMM ||
                node->children[1]->type == NodeType::OPERAND_LABEL) {
                has_imm = true;
                imm = get_immediate(node->children[1]);
            } else {
                rm = get_register(node->children[1], nullptr);
                if (rm < 0) {
                    error("Invalid source register");
                    return 0;
                }
            }
        } else {
            rn = get_register(node->children[1], nullptr);
            if (rn < 0) {
                error("Invalid first source register");
                return 0;
            }
            
            if (node->children[2]->type == NodeType::OPERAND_IMM ||
                node->children[2]->type == NodeType::OPERAND_LABEL) {
                has_imm = true;
                imm = get_immediate(node->children[2]);
            } else {
                rm = get_register(node->children[2], nullptr);
                if (rm < 0) {
                    error("Invalid second source register");
                    return 0;
                }
            }
        }
    }
    
    // Use immediate form if applicable
    if (has_imm) {
        return encode_data_proc_imm(node);
    }
    
    // Encode register form
    uint32_t sf = is_64bit ? 1 : 0;
    uint32_t encoding = 0;
    
    // ADD (shifted register): sf 0 0 01011 shift 0 Rm imm6 Rn Rd
    if (token_equals(mnemonic, "add")) {
        encoding = (sf << 31) | (0x0B << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // ADDS (shifted register): sf 0 1 01011 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "adds")) {
        encoding = (sf << 31) | (0x2B << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // SUB (shifted register): sf 1 0 01011 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "sub") || token_equals(mnemonic, "neg")) {
        encoding = (sf << 31) | (0x4B << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // SUBS (shifted register): sf 1 1 01011 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "subs") || token_equals(mnemonic, "cmp")) {
        encoding = (sf << 31) | (0x6B << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // AND (shifted register): sf 00 01010 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "and")) {
        encoding = (sf << 31) | (0x0A << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // ANDS (shifted register): sf 11 01010 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "ands") || token_equals(mnemonic, "tst")) {
        encoding = (sf << 31) | (0x6A << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // ORR (shifted register): sf 01 01010 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "orr") || token_equals(mnemonic, "mov")) {
        encoding = (sf << 31) | (0x2A << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // EOR (shifted register): sf 10 01010 shift 0 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "eor")) {
        encoding = (sf << 31) | (0x4A << 24) | (rm << 16) | (rn << 5) | rd;
    }
    // BIC (shifted register): sf 00 01010 shift 1 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "bic")) {
        encoding = (sf << 31) | (0x0A << 24) | (1 << 21) | (rm << 16) | (rn << 5) | rd;
    }
    // ORN (shifted register): sf 01 01010 shift 1 Rm imm6 Rn Rd
    else if (token_equals(mnemonic, "orn") || token_equals(mnemonic, "mvn")) {
        encoding = (sf << 31) | (0x2A << 24) | (1 << 21) | (rm << 16) | (rn << 5) | rd;
    }
    // MUL: sf 00 11011 000 Rm 0 11111 Rn Rd (MADD with Ra=XZR)
    else if (token_equals(mnemonic, "mul")) {
        encoding = (sf << 31) | (0x1B << 24) | (rm << 16) | (0x1F << 10) | (rn << 5) | rd;
    }
    // UDIV: sf 0 0 11010110 Rm 00001 0 Rn Rd
    else if (token_equals(mnemonic, "udiv")) {
        encoding = (sf << 31) | (0x1AC << 21) | (rm << 16) | (0x02 << 10) | (rn << 5) | rd;
    }
    // SDIV: sf 0 0 11010110 Rm 00001 1 Rn Rd
    else if (token_equals(mnemonic, "sdiv")) {
        encoding = (sf << 31) | (0x1AC << 21) | (rm << 16) | (0x03 << 10) | (rn << 5) | rd;
    }
    // LSL (register): sf 0 0 11010110 Rm 0010 00 Rn Rd
    else if (token_equals(mnemonic, "lsl")) {
        encoding = (sf << 31) | (0x1AC << 21) | (rm << 16) | (0x08 << 10) | (rn << 5) | rd;
    }
    // LSR (register): sf 0 0 11010110 Rm 0010 01 Rn Rd
    else if (token_equals(mnemonic, "lsr")) {
        encoding = (sf << 31) | (0x1AC << 21) | (rm << 16) | (0x09 << 10) | (rn << 5) | rd;
    }
    // ASR (register): sf 0 0 11010110 Rm 0010 10 Rn Rd
    else if (token_equals(mnemonic, "asr")) {
        encoding = (sf << 31) | (0x1AC << 21) | (rm << 16) | (0x0A << 10) | (rn << 5) | rd;
    }
    // ROR (register): sf 0 0 11010110 Rm 0010 11 Rn Rd
    else if (token_equals(mnemonic, "ror")) {
        encoding = (sf << 31) | (0x1AC << 21) | (rm << 16) | (0x0B << 10) | (rn << 5) | rd;
    }
    else {
        error("Unsupported data processing instruction");
        return 0;
    }
    
    return encoding;
}

/*
 * Encode data processing immediate instruction
 */
uint32_t CodeGenerator::encode_data_proc_imm(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    bool is_64bit = true;
    int rd = get_register(node->children[0], &is_64bit);
    int rn = -1;
    int64_t imm = 0;
    
    // Handle CMP, CMN (2 operands)
    if (token_equals(mnemonic, "cmp") || token_equals(mnemonic, "cmn")) {
        rn = rd;
        rd = 31;  // XZR
        imm = get_immediate(node->children[1]);
    }
    // Standard 3-operand or 2-operand form
    else if (node->child_count >= 3) {
        rn = get_register(node->children[1], nullptr);
        imm = get_immediate(node->children[2]);
    } else {
        rn = rd;
        imm = get_immediate(node->children[1]);
    }
    
    if (rd < 0 || rn < 0) {
        error("Invalid register in immediate instruction");
        return 0;
    }
    
    uint32_t sf = is_64bit ? 1 : 0;
    uint32_t encoding = 0;
    
    // Check if immediate fits in 12 bits
    if (imm < 0 || imm > 4095) {
        // Try shifted immediate (imm << 12)
        if ((imm & 0xFFF) == 0 && (imm >> 12) <= 4095) {
            imm = imm >> 12;
            // Set shift bit
            // ADD/SUB immediate: sf op S 100010 sh imm12 Rn Rd
            if (token_equals(mnemonic, "add")) {
                encoding = (sf << 31) | (0x11 << 24) | (1 << 22) | ((imm & 0xFFF) << 10) | (rn << 5) | rd;
            } else if (token_equals(mnemonic, "sub") || token_equals(mnemonic, "cmp")) {
                encoding = (sf << 31) | (0x51 << 24) | (1 << 22) | ((imm & 0xFFF) << 10) | (rn << 5) | rd;
            } else {
                error("Immediate value out of range");
                return 0;
            }
            return encoding;
        }
        error("Immediate value out of range");
        return 0;
    }
    
    // ADD immediate: sf 0 0 100010 sh imm12 Rn Rd
    if (token_equals(mnemonic, "add")) {
        encoding = (sf << 31) | (0x11 << 24) | ((imm & 0xFFF) << 10) | (rn << 5) | rd;
    }
    // ADDS immediate: sf 0 1 100010 sh imm12 Rn Rd
    else if (token_equals(mnemonic, "adds") || token_equals(mnemonic, "cmn")) {
        encoding = (sf << 31) | (0x31 << 24) | ((imm & 0xFFF) << 10) | (rn << 5) | rd;
    }
    // SUB immediate: sf 1 0 100010 sh imm12 Rn Rd
    else if (token_equals(mnemonic, "sub")) {
        encoding = (sf << 31) | (0x51 << 24) | ((imm & 0xFFF) << 10) | (rn << 5) | rd;
    }
    // SUBS immediate: sf 1 1 100010 sh imm12 Rn Rd
    else if (token_equals(mnemonic, "subs") || token_equals(mnemonic, "cmp")) {
        encoding = (sf << 31) | (0x71 << 24) | ((imm & 0xFFF) << 10) | (rn << 5) | rd;
    }
    else {
        error("Instruction does not support immediate form");
        return 0;
    }
    
    return encoding;
}


/*
 * Encode load/store instruction
 * Requirements: 7.3
 */
uint32_t CodeGenerator::encode_load_store(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    if (node->child_count < 2) {
        error("Load/store instruction requires register and memory operand");
        return 0;
    }
    
    bool is_64bit = true;
    int rt = get_register(node->children[0], &is_64bit);
    if (rt < 0) {
        error("Invalid register for load/store");
        return 0;
    }
    
    // Check for label (PC-relative LDR)
    if (node->children[1]->type == NodeType::OPERAND_LABEL) {
        int64_t target = resolve_label(node->children[1]);
        int64_t offset = target - code_offset_;
        
        // LDR (literal): opc 01 1 V 00 imm19 Rt
        // offset must be within ±1MB and 4-byte aligned
        if (offset < -1048576 || offset > 1048572 || (offset & 3) != 0) {
            error("PC-relative offset out of range");
            return 0;
        }
        
        int64_t imm19 = (offset >> 2) & 0x7FFFF;
        uint32_t opc = is_64bit ? 1 : 0;
        return (opc << 30) | (0x18 << 24) | (imm19 << 5) | rt;
    }
    
    // Memory operand
    if (node->children[1]->type != NodeType::OPERAND_MEM) {
        error("Expected memory operand");
        return 0;
    }
    
    int base = -1;
    int64_t offset = 0;
    bool pre_index = false;
    bool post_index = false;
    
    if (!get_memory_operand(node->children[1], &base, &offset, &pre_index, &post_index)) {
        error("Invalid memory operand");
        return 0;
    }
    
    // Determine size and operation
    uint32_t size = 3;  // 64-bit default
    uint32_t opc = 1;   // Load default
    bool is_signed = false;
    
    if (token_equals(mnemonic, "ldr")) {
        size = is_64bit ? 3 : 2;
        opc = 1;
    } else if (token_equals(mnemonic, "str")) {
        size = is_64bit ? 3 : 2;
        opc = 0;
    } else if (token_equals(mnemonic, "ldrb")) {
        size = 0;
        opc = 1;
    } else if (token_equals(mnemonic, "strb")) {
        size = 0;
        opc = 0;
    } else if (token_equals(mnemonic, "ldrh")) {
        size = 1;
        opc = 1;
    } else if (token_equals(mnemonic, "strh")) {
        size = 1;
        opc = 0;
    } else if (token_equals(mnemonic, "ldrsb")) {
        size = 0;
        opc = is_64bit ? 2 : 3;
        is_signed = true;
    } else if (token_equals(mnemonic, "ldrsh")) {
        size = 1;
        opc = is_64bit ? 2 : 3;
        is_signed = true;
    } else if (token_equals(mnemonic, "ldrsw")) {
        size = 2;
        opc = 2;
        is_signed = true;
    }
    
    uint32_t encoding = 0;
    
    // Pre-index or post-index form
    if (pre_index || post_index) {
        // size 11 1 V 00 opc 0 imm9 idx Rn Rt
        // idx: 01 = post-index, 11 = pre-index
        if (offset < -256 || offset > 255) {
            error("Pre/post-index offset out of range (-256 to 255)");
            return 0;
        }
        
        uint32_t imm9 = offset & 0x1FF;
        uint32_t idx = pre_index ? 3 : 1;
        
        encoding = (size << 30) | (0x38 << 24) | (opc << 22) | (imm9 << 12) | 
                   (idx << 10) | (base << 5) | rt;
    }
    // Unsigned offset form
    else {
        // size 11 1 V 01 opc imm12 Rn Rt
        // Scale offset by access size
        int scale = size;
        if (is_signed && size < 2) scale = is_64bit ? 3 : 2;
        
        int64_t scaled_offset = offset >> scale;
        if ((offset & ((1 << scale) - 1)) != 0) {
            error("Offset not aligned to access size");
            return 0;
        }
        if (scaled_offset < 0 || scaled_offset > 4095) {
            error("Unsigned offset out of range");
            return 0;
        }
        
        encoding = (size << 30) | (0x39 << 24) | (opc << 22) | 
                   (scaled_offset << 10) | (base << 5) | rt;
    }
    
    return encoding;
}

/*
 * Encode load/store pair instruction
 * Requirements: 7.3
 */
uint32_t CodeGenerator::encode_load_store_pair(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    if (node->child_count < 3) {
        error("Load/store pair requires two registers and memory operand");
        return 0;
    }
    
    bool is_64bit = true;
    int rt1 = get_register(node->children[0], &is_64bit);
    int rt2 = get_register(node->children[1], nullptr);
    
    if (rt1 < 0 || rt2 < 0) {
        error("Invalid registers for load/store pair");
        return 0;
    }
    
    if (node->children[2]->type != NodeType::OPERAND_MEM) {
        error("Expected memory operand");
        return 0;
    }
    
    int base = -1;
    int64_t offset = 0;
    bool pre_index = false;
    bool post_index = false;
    
    if (!get_memory_operand(node->children[2], &base, &offset, &pre_index, &post_index)) {
        error("Invalid memory operand");
        return 0;
    }
    
    // Scale offset by register size (8 for 64-bit, 4 for 32-bit)
    int scale = is_64bit ? 3 : 2;
    int64_t scaled_offset = offset >> scale;
    
    if ((offset & ((1 << scale) - 1)) != 0) {
        error("Offset not aligned to register size");
        return 0;
    }
    if (scaled_offset < -64 || scaled_offset > 63) {
        error("Pair offset out of range");
        return 0;
    }
    
    uint32_t opc = is_64bit ? 2 : 0;
    uint32_t L = token_equals(mnemonic, "ldp") ? 1 : 0;
    uint32_t imm7 = scaled_offset & 0x7F;
    
    uint32_t encoding = 0;
    
    if (post_index) {
        // opc 0 101 0 001 L imm7 Rt2 Rn Rt
        encoding = (opc << 30) | (0x28 << 23) | (L << 22) | (imm7 << 15) | 
                   (rt2 << 10) | (base << 5) | rt1;
    } else if (pre_index) {
        // opc 0 101 0 011 L imm7 Rt2 Rn Rt
        encoding = (opc << 30) | (0x29 << 23) | (1 << 24) | (L << 22) | (imm7 << 15) | 
                   (rt2 << 10) | (base << 5) | rt1;
    } else {
        // Signed offset: opc 0 101 0 010 L imm7 Rt2 Rn Rt
        encoding = (opc << 30) | (0x29 << 23) | (L << 22) | (imm7 << 15) | 
                   (rt2 << 10) | (base << 5) | rt1;
    }
    
    return encoding;
}

/*
 * Encode unconditional branch
 * Requirements: 7.4
 */
uint32_t CodeGenerator::encode_branch(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    if (node->child_count < 1) {
        error("Branch requires target operand");
        return 0;
    }
    
    int64_t target = 0;
    
    if (node->children[0]->type == NodeType::OPERAND_LABEL) {
        target = resolve_label(node->children[0]);
    } else if (node->children[0]->type == NodeType::OPERAND_IMM) {
        target = get_immediate(node->children[0]);
    } else {
        error("Invalid branch target");
        return 0;
    }
    
    int64_t offset = target - code_offset_;
    
    // Check range: ±128MB for B/BL
    if (offset < -134217728 || offset > 134217724) {
        error("Branch target out of range");
        return 0;
    }
    
    // Must be 4-byte aligned
    if ((offset & 3) != 0) {
        error("Branch target not aligned");
        return 0;
    }
    
    int64_t imm26 = (offset >> 2) & 0x3FFFFFF;
    
    // B: 0 00101 imm26
    // BL: 1 00101 imm26
    uint32_t op = token_equals(mnemonic, "bl") ? 1 : 0;
    
    return (op << 31) | (0x05 << 26) | imm26;
}

/*
 * Encode conditional branch
 * Requirements: 7.4
 */
uint32_t CodeGenerator::encode_branch_cond(ASTNode* node, int cond) {
    if (node->child_count < 1) {
        error("Conditional branch requires target operand");
        return 0;
    }
    
    int64_t target = 0;
    
    if (node->children[0]->type == NodeType::OPERAND_LABEL) {
        target = resolve_label(node->children[0]);
    } else if (node->children[0]->type == NodeType::OPERAND_IMM) {
        target = get_immediate(node->children[0]);
    } else {
        error("Invalid branch target");
        return 0;
    }
    
    int64_t offset = target - code_offset_;
    
    // Check range: ±1MB for conditional branches
    if (offset < -1048576 || offset > 1048572) {
        error("Conditional branch target out of range");
        return 0;
    }
    
    // Must be 4-byte aligned
    if ((offset & 3) != 0) {
        error("Branch target not aligned");
        return 0;
    }
    
    int64_t imm19 = (offset >> 2) & 0x7FFFF;
    
    // B.cond: 0101010 0 imm19 0 cond
    return (0x54 << 24) | (imm19 << 5) | cond;
}

/*
 * Encode CBZ/CBNZ instruction
 * Requirements: 7.4
 */
uint32_t CodeGenerator::encode_cbz_cbnz(ASTNode* node, bool is_cbnz) {
    if (node->child_count < 2) {
        error("CBZ/CBNZ requires register and target operand");
        return 0;
    }
    
    bool is_64bit = true;
    int rt = get_register(node->children[0], &is_64bit);
    if (rt < 0) {
        error("Invalid register for CBZ/CBNZ");
        return 0;
    }
    
    int64_t target = 0;
    
    if (node->children[1]->type == NodeType::OPERAND_LABEL) {
        target = resolve_label(node->children[1]);
    } else if (node->children[1]->type == NodeType::OPERAND_IMM) {
        target = get_immediate(node->children[1]);
    } else {
        error("Invalid branch target");
        return 0;
    }
    
    int64_t offset = target - code_offset_;
    
    // Check range: ±1MB
    if (offset < -1048576 || offset > 1048572) {
        error("CBZ/CBNZ target out of range");
        return 0;
    }
    
    // Must be 4-byte aligned
    if ((offset & 3) != 0) {
        error("Branch target not aligned");
        return 0;
    }
    
    int64_t imm19 = (offset >> 2) & 0x7FFFF;
    
    // CBZ: sf 011010 0 imm19 Rt
    // CBNZ: sf 011010 1 imm19 Rt
    uint32_t sf = is_64bit ? 1 : 0;
    uint32_t op = is_cbnz ? 1 : 0;
    
    return (sf << 31) | (0x34 << 24) | (op << 24) | (imm19 << 5) | rt;
}


/*
 * Encode MOV instruction variants
 * Requirements: 7.2
 */
uint32_t CodeGenerator::encode_mov(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    if (node->child_count < 2) {
        error("MOV requires destination and source operand");
        return 0;
    }
    
    bool is_64bit = true;
    int rd = get_register(node->children[0], &is_64bit);
    if (rd < 0) {
        error("Invalid destination register for MOV");
        return 0;
    }
    
    // MOV (register) - alias for ORR Rd, XZR, Rm
    if (node->children[1]->type == NodeType::OPERAND_REG) {
        int rm = get_register(node->children[1], nullptr);
        if (rm < 0) {
            error("Invalid source register for MOV");
            return 0;
        }
        
        uint32_t sf = is_64bit ? 1 : 0;
        // ORR (shifted register): sf 01 01010 00 0 Rm 000000 11111 Rd
        return (sf << 31) | (0x2A << 24) | (rm << 16) | (0x1F << 5) | rd;
    }
    
    // MOV (immediate) variants
    int64_t imm = get_immediate(node->children[1]);
    uint32_t sf = is_64bit ? 1 : 0;
    
    // MOVZ - move wide with zero
    if (token_equals(mnemonic, "movz") || 
        (token_equals(mnemonic, "mov") && imm >= 0 && imm <= 0xFFFF)) {
        // MOVZ: sf 10 100101 hw imm16 Rd
        uint32_t hw = 0;  // Shift amount (0, 16, 32, 48)
        uint32_t imm16 = imm & 0xFFFF;
        
        // Check if we need to shift
        if (imm > 0xFFFF) {
            if ((imm & 0xFFFF) == 0 && (imm >> 16) <= 0xFFFF) {
                hw = 1;
                imm16 = (imm >> 16) & 0xFFFF;
            } else if ((imm & 0xFFFFFFFF) == 0 && (imm >> 32) <= 0xFFFF) {
                hw = 2;
                imm16 = (imm >> 32) & 0xFFFF;
            } else if ((imm & 0xFFFFFFFFFFFF) == 0) {
                hw = 3;
                imm16 = (imm >> 48) & 0xFFFF;
            } else {
                error("Immediate value cannot be encoded in MOVZ");
                return 0;
            }
        }
        
        return (sf << 31) | (0xA5 << 23) | (hw << 21) | (imm16 << 5) | rd;
    }
    
    // MOVN - move wide with NOT
    if (token_equals(mnemonic, "movn") ||
        (token_equals(mnemonic, "mov") && imm < 0)) {
        // MOVN: sf 00 100101 hw imm16 Rd
        uint64_t not_imm = ~imm;
        uint32_t hw = 0;
        uint32_t imm16 = not_imm & 0xFFFF;
        
        if (not_imm > 0xFFFF) {
            if ((not_imm & 0xFFFF) == 0 && (not_imm >> 16) <= 0xFFFF) {
                hw = 1;
                imm16 = (not_imm >> 16) & 0xFFFF;
            } else {
                error("Immediate value cannot be encoded in MOVN");
                return 0;
            }
        }
        
        return (sf << 31) | (0x25 << 23) | (hw << 21) | (imm16 << 5) | rd;
    }
    
    // MOVK - move wide with keep
    if (token_equals(mnemonic, "movk")) {
        // MOVK: sf 11 100101 hw imm16 Rd
        uint32_t hw = 0;
        uint32_t imm16 = imm & 0xFFFF;
        
        // Check for shift specifier in operands
        if (node->child_count > 2) {
            // TODO: Parse LSL #n
        }
        
        return (sf << 31) | (0xE5 << 23) | (hw << 21) | (imm16 << 5) | rd;
    }
    
    error("Cannot encode MOV instruction");
    return 0;
}

/*
 * Encode system instruction
 * Requirements: 7.5
 */
uint32_t CodeGenerator::encode_system(ASTNode* node) {
    const Token& mnemonic = node->token;
    
    // WFI: Wait for interrupt
    if (token_equals(mnemonic, "wfi")) {
        return 0xD503207F;
    }
    
    // WFE: Wait for event
    if (token_equals(mnemonic, "wfe")) {
        return 0xD503205F;
    }
    
    // SEV: Send event
    if (token_equals(mnemonic, "sev")) {
        return 0xD503209F;
    }
    
    // SEVL: Send event local
    if (token_equals(mnemonic, "sevl")) {
        return 0xD50320BF;
    }
    
    // DMB: Data memory barrier
    if (token_equals(mnemonic, "dmb")) {
        uint32_t option = 0xF;  // SY (full system)
        if (node->child_count > 0 && node->children[0]->type == NodeType::OPERAND_IMM) {
            option = get_immediate(node->children[0]) & 0xF;
        }
        return 0xD50330BF | (option << 8);
    }
    
    // DSB: Data synchronization barrier
    if (token_equals(mnemonic, "dsb")) {
        uint32_t option = 0xF;  // SY
        if (node->child_count > 0 && node->children[0]->type == NodeType::OPERAND_IMM) {
            option = get_immediate(node->children[0]) & 0xF;
        }
        return 0xD503309F | (option << 8);
    }
    
    // ISB: Instruction synchronization barrier
    if (token_equals(mnemonic, "isb")) {
        uint32_t option = 0xF;  // SY
        if (node->child_count > 0 && node->children[0]->type == NodeType::OPERAND_IMM) {
            option = get_immediate(node->children[0]) & 0xF;
        }
        return 0xD50330DF | (option << 8);
    }
    
    // SVC: Supervisor call
    if (token_equals(mnemonic, "svc")) {
        uint32_t imm16 = 0;
        if (node->child_count > 0) {
            imm16 = get_immediate(node->children[0]) & 0xFFFF;
        }
        return 0xD4000001 | (imm16 << 5);
    }
    
    // HVC: Hypervisor call
    if (token_equals(mnemonic, "hvc")) {
        uint32_t imm16 = 0;
        if (node->child_count > 0) {
            imm16 = get_immediate(node->children[0]) & 0xFFFF;
        }
        return 0xD4000002 | (imm16 << 5);
    }
    
    // SMC: Secure monitor call
    if (token_equals(mnemonic, "smc")) {
        uint32_t imm16 = 0;
        if (node->child_count > 0) {
            imm16 = get_immediate(node->children[0]) & 0xFFFF;
        }
        return 0xD4000003 | (imm16 << 5);
    }
    
    error("Unknown system instruction");
    return 0;
}

} // namespace casm
