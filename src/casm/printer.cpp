/*
 * EmberOS CASM Pretty Printer Implementation
 * Converts AST back to C.ASM source text
 * 
 * Requirements: 8.7
 */

#include "casm/printer.h"

namespace casm {

// String helper functions (freestanding environment)
static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

/*
 * Constructor
 */
Printer::Printer()
    : output_(nullptr)
    , max_len_(0)
    , pos_(0)
    , has_error_(false)
{
}

/*
 * Write a single character to output
 */
void Printer::write_char(char c) {
    if (has_error_) return;
    if (pos_ >= max_len_ - 1) {
        has_error_ = true;
        return;
    }
    output_[pos_++] = c;
}

/*
 * Write a null-terminated string to output
 */
void Printer::write_string(const char* str) {
    if (has_error_ || !str) return;
    while (*str != '\0') {
        write_char(*str++);
    }
}

/*
 * Write a string with specified length to output
 */
void Printer::write_string_n(const char* str, size_t len) {
    if (has_error_ || !str) return;
    for (size_t i = 0; i < len; i++) {
        write_char(str[i]);
    }
}

/*
 * Write a decimal number to output
 */
void Printer::write_number(int64_t value) {
    if (has_error_) return;
    
    if (value < 0) {
        write_char('-');
        value = -value;
    }
    
    if (value == 0) {
        write_char('0');
        return;
    }
    
    // Convert to string (reverse order)
    char buf[24];
    int len = 0;
    uint64_t uval = static_cast<uint64_t>(value);
    
    while (uval > 0) {
        buf[len++] = '0' + (uval % 10);
        uval /= 10;
    }
    
    // Write in correct order
    while (len > 0) {
        write_char(buf[--len]);
    }
}

/*
 * Write a hexadecimal number to output (with 0x prefix)
 */
void Printer::write_hex(uint64_t value) {
    if (has_error_) return;
    
    write_string("0x");
    
    if (value == 0) {
        write_char('0');
        return;
    }
    
    // Convert to hex string (reverse order)
    char buf[20];
    int len = 0;
    static const char hex_chars[] = "0123456789abcdef";
    
    while (value > 0) {
        buf[len++] = hex_chars[value & 0xF];
        value >>= 4;
    }
    
    // Write in correct order
    while (len > 0) {
        write_char(buf[--len]);
    }
}

/*
 * Write a newline to output
 */
void Printer::write_newline() {
    write_char('\n');
}

/*
 * Write a space to output
 */
void Printer::write_space() {
    write_char(' ');
}

/*
 * Write a comma followed by space
 */
void Printer::write_comma() {
    write_char(',');
    write_char(' ');
}

/*
 * Write register name based on number and size
 */
void Printer::write_register_name(int reg_num, bool is_64bit) {
    if (reg_num == 31) {
        // Could be SP, XZR, or WZR depending on context
        // For simplicity, use sp for 64-bit reg 31, wzr for 32-bit
        if (is_64bit) {
            write_string("sp");
        } else {
            write_string("wzr");
        }
    } else if (reg_num == 30 && is_64bit) {
        // Link register alias
        write_char(is_64bit ? 'x' : 'w');
        write_number(reg_num);
    } else {
        write_char(is_64bit ? 'x' : 'w');
        write_number(reg_num);
    }
}

/*
 * Convert AST back to C.ASM source text
 * Requirements: 8.7
 */
size_t Printer::print(ASTNode* ast, char* output, size_t max_len) {
    if (!ast || !output || max_len == 0) {
        return 0;
    }
    
    output_ = output;
    max_len_ = max_len;
    pos_ = 0;
    has_error_ = false;
    
    print_node(ast);
    
    // Null-terminate
    if (pos_ < max_len_) {
        output_[pos_] = '\0';
    } else if (max_len_ > 0) {
        output_[max_len_ - 1] = '\0';
    }
    
    return pos_;
}

/*
 * Print any AST node (dispatcher)
 */
void Printer::print_node(ASTNode* node) {
    if (!node || has_error_) return;
    
    switch (node->type) {
        case NodeType::PROGRAM:
            print_program(node);
            break;
        case NodeType::LABEL:
            print_label(node);
            break;
        case NodeType::INSTRUCTION:
            print_instruction(node);
            break;
        case NodeType::DIRECTIVE:
            print_directive(node);
            break;
        case NodeType::OPERAND_REG:
            print_register(node);
            break;
        case NodeType::OPERAND_IMM:
            print_immediate(node);
            break;
        case NodeType::OPERAND_LABEL:
            print_label_operand(node);
            break;
        case NodeType::OPERAND_MEM:
            print_memory_operand(node);
            break;
    }
}

/*
 * Print program node (list of statements)
 */
void Printer::print_program(ASTNode* node) {
    if (!node || has_error_) return;
    
    // Use statement array for PROGRAM nodes
    for (int i = 0; i < node->statement_count; i++) {
        print_node(node->statements[i]);
        write_newline();
    }
}

/*
 * Print label definition
 * Requirements: 8.2
 */
void Printer::print_label(ASTNode* node) {
    if (!node || has_error_) return;
    
    // Write label name from token
    write_string_n(node->token.start, node->token.length);
    write_char(':');
}

/*
 * Print instruction with operands
 * Requirements: 7.2, 7.3, 7.4, 7.5
 */
void Printer::print_instruction(ASTNode* node) {
    if (!node || has_error_) return;
    
    // Write mnemonic (convert to lowercase for consistency)
    for (size_t i = 0; i < node->token.length; i++) {
        write_char(to_lower(node->token.start[i]));
    }
    
    // Write operands
    if (node->child_count > 0) {
        write_space();
        
        for (int i = 0; i < node->child_count; i++) {
            if (i > 0) {
                write_comma();
            }
            print_operand(node->children[i]);
        }
    }
}

/*
 * Print directive
 * Requirements: 7.8
 */
void Printer::print_directive(ASTNode* node) {
    if (!node || has_error_) return;
    
    // Write dot prefix
    write_char('.');
    
    // Write directive name (convert to lowercase)
    for (size_t i = 0; i < node->token.length; i++) {
        write_char(to_lower(node->token.start[i]));
    }
    
    // Write arguments
    if (node->child_count > 0) {
        write_space();
        
        for (int i = 0; i < node->child_count; i++) {
            if (i > 0) {
                write_comma();
            }
            print_operand(node->children[i]);
        }
    }
}

/*
 * Print any operand type
 */
void Printer::print_operand(ASTNode* node) {
    if (!node || has_error_) return;
    
    switch (node->type) {
        case NodeType::OPERAND_REG:
            print_register(node);
            break;
        case NodeType::OPERAND_IMM:
            print_immediate(node);
            break;
        case NodeType::OPERAND_LABEL:
            print_label_operand(node);
            break;
        case NodeType::OPERAND_MEM:
            print_memory_operand(node);
            break;
        default:
            // For other node types used as operands (e.g., in directives)
            write_string_n(node->token.start, node->token.length);
            break;
    }
}

/*
 * Print register operand
 * Requirements: 8.3
 */
void Printer::print_register(ASTNode* node) {
    if (!node || has_error_) return;
    
    write_register_name(node->data.reg_num, node->is_64bit);
}

/*
 * Print immediate operand
 * Requirements: 8.4
 */
void Printer::print_immediate(ASTNode* node) {
    if (!node || has_error_) return;
    
    write_char('#');
    write_number(node->data.imm_value);
}

/*
 * Print label reference operand
 */
void Printer::print_label_operand(ASTNode* node) {
    if (!node || has_error_) return;
    
    // Write label name from token
    write_string_n(node->token.start, node->token.length);
}

/*
 * Print memory operand
 * Requirements: 8.5
 */
void Printer::print_memory_operand(ASTNode* node) {
    if (!node || has_error_) return;
    
    const MemOperand& mem = node->data.mem;
    
    write_char('[');
    write_register_name(mem.base_reg, mem.is_64bit);
    
    // Handle different addressing modes
    if (mem.post_index) {
        // Post-index: [Xn], #offset
        write_char(']');
        write_comma();
        write_char('#');
        write_number(mem.offset);
    } else {
        // Pre-index or offset mode
        if (mem.index_reg >= 0) {
            // Register offset: [Xn, Xm]
            write_comma();
            write_register_name(mem.index_reg, mem.is_64bit);
        } else if (mem.offset != 0) {
            // Immediate offset: [Xn, #offset]
            write_comma();
            write_char('#');
            write_number(mem.offset);
        }
        
        write_char(']');
        
        // Pre-index indicator
        if (mem.pre_index) {
            write_char('!');
        }
    }
}

} // namespace casm
