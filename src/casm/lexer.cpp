/*
 * EmberOS CASM Lexer Implementation
 * Tokenizer for C.ASM assembly language
 * 
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5
 */

#include "casm/lexer.h"

namespace casm {

// String length helper (since we can't use standard library)
static size_t str_len(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') len++;
    return len;
}

// Convert character to lowercase
static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

/*
 * Construct a lexer for the given source code
 */
Lexer::Lexer(const char* source)
    : source_(source)
    , current_(source)
    , start_(source)
    , line_(1)
{
}

/*
 * Advance to next character and return the current one
 */
char Lexer::advance() {
    return *current_++;
}

/*
 * Peek at current character without advancing
 */
char Lexer::peek() const {
    return *current_;
}

/*
 * Peek at next character without advancing
 */
char Lexer::peek_next() const {
    if (is_at_end()) return '\0';
    return current_[1];
}

/*
 * Match expected character and advance if matched
 */
bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (*current_ != expected) return false;
    current_++;
    return true;
}

/*
 * Skip whitespace (spaces and tabs, but not newlines)
 */
void Lexer::skip_whitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            default:
                return;
        }
    }
}

/*
 * Skip line comment (from ; or // to end of line)
 * Requirements: 8.1
 */
void Lexer::skip_line_comment() {
    while (peek() != '\n' && !is_at_end()) {
        advance();
    }
}

/*
 * Create a token of the given type
 */
Token Lexer::make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = start_;
    token.length = static_cast<size_t>(current_ - start_);
    token.line = line_;
    token.number_value = 0;
    return token;
}

/*
 * Create an error token with message
 */
Token Lexer::make_error_token(const char* message) {
    Token token;
    token.type = TokenType::ERROR;
    token.start = message;
    token.length = str_len(message);
    token.line = line_;
    token.number_value = 0;
    return token;
}

/*
 * Character classification helpers
 */
bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::is_alnum(char c) const {
    return is_alpha(c) || is_digit(c);
}

bool Lexer::is_hex_digit(char c) const {
    return is_digit(c) ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

bool Lexer::is_binary_digit(char c) const {
    return c == '0' || c == '1';
}

/*
 * Parse decimal number
 * Requirements: 7.7
 */
int64_t Lexer::parse_decimal() {
    int64_t value = 0;
    bool negative = false;
    
    const char* p = start_;
    if (*p == '-') {
        negative = true;
        p++;
    }
    
    while (p < current_) {
        value = value * 10 + (*p - '0');
        p++;
    }
    
    return negative ? -value : value;
}

/*
 * Parse hexadecimal number (0x prefix already consumed)
 * Requirements: 7.7
 */
int64_t Lexer::parse_hexadecimal() {
    int64_t value = 0;
    const char* p = start_ + 2; // Skip "0x"
    
    while (p < current_) {
        char c = *p;
        int digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            break;
        }
        value = value * 16 + digit;
        p++;
    }
    
    return value;
}

/*
 * Parse binary number (0b prefix already consumed)
 * Requirements: 7.7
 */
int64_t Lexer::parse_binary() {
    int64_t value = 0;
    const char* p = start_ + 2; // Skip "0b"
    
    while (p < current_) {
        value = value * 2 + (*p - '0');
        p++;
    }
    
    return value;
}

/*
 * Scan a number token (decimal, hex, or binary)
 * Requirements: 7.7
 */
Token Lexer::make_number_token() {
    // Check for hex (0x) or binary (0b) prefix
    if (start_[0] == '0' && (current_ - start_) > 1) {
        char prefix = to_lower(start_[1]);
        if (prefix == 'x') {
            // Hexadecimal
            Token token = make_token(TokenType::NUMBER);
            token.number_value = parse_hexadecimal();
            return token;
        } else if (prefix == 'b') {
            // Binary
            Token token = make_token(TokenType::NUMBER);
            token.number_value = parse_binary();
            return token;
        }
    }
    
    // Decimal
    Token token = make_token(TokenType::NUMBER);
    token.number_value = parse_decimal();
    return token;
}

/*
 * Scan an identifier token
 * Requirements: 8.2, 8.3
 */
Token Lexer::make_identifier_token() {
    return make_token(TokenType::IDENTIFIER);
}

/*
 * Scan a string literal token
 */
Token Lexer::make_string_token() {
    // Opening quote already consumed
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            return make_error_token("Unterminated string");
        }
        if (peek() == '\\' && peek_next() != '\0') {
            advance(); // Skip escape character
        }
        advance();
    }
    
    if (is_at_end()) {
        return make_error_token("Unterminated string");
    }
    
    advance(); // Closing quote
    return make_token(TokenType::STRING);
}

/*
 * Get the next token from the source
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5
 */
Token Lexer::next_token() {
    skip_whitespace();
    start_ = current_;
    
    if (is_at_end()) {
        return make_token(TokenType::END_OF_FILE);
    }
    
    char c = advance();
    
    // Check for comments
    // Semicolon comment (Requirement 8.1)
    if (c == ';') {
        skip_line_comment();
        start_ = current_;
        // Return newline or EOF after comment
        if (is_at_end()) {
            return make_token(TokenType::END_OF_FILE);
        }
        if (peek() == '\n') {
            advance();
            line_++;
            return make_token(TokenType::NEWLINE);
        }
        return next_token();
    }
    
    // Double-slash comment (Requirement 8.1)
    if (c == '/' && peek() == '/') {
        advance(); // consume second /
        skip_line_comment();
        start_ = current_;
        if (is_at_end()) {
            return make_token(TokenType::END_OF_FILE);
        }
        if (peek() == '\n') {
            advance();
            line_++;
            return make_token(TokenType::NEWLINE);
        }
        return next_token();
    }
    
    // Newline
    if (c == '\n') {
        line_++;
        return make_token(TokenType::NEWLINE);
    }
    
    // Single character tokens
    switch (c) {
        case ':': return make_token(TokenType::COLON);
        case ',': return make_token(TokenType::COMMA);
        case '#': return make_token(TokenType::HASH);
        case '[': return make_token(TokenType::LBRACKET);
        case ']': return make_token(TokenType::RBRACKET);
        case '!': return make_token(TokenType::EXCLAIM);
        case '.': return make_token(TokenType::DOT);
        case '+': return make_token(TokenType::PLUS);
    }
    
    // Minus can be part of a negative number or standalone
    if (c == '-') {
        // Check if followed by a digit (negative number)
        if (is_digit(peek())) {
            while (is_digit(peek())) advance();
            return make_number_token();
        }
        return make_token(TokenType::MINUS);
    }
    
    // Numbers (Requirement 7.7)
    if (is_digit(c)) {
        // Check for hex (0x) or binary (0b)
        if (c == '0') {
            char next = to_lower(peek());
            if (next == 'x') {
                advance(); // consume 'x'
                while (is_hex_digit(peek())) advance();
                return make_number_token();
            } else if (next == 'b') {
                advance(); // consume 'b'
                while (is_binary_digit(peek())) advance();
                return make_number_token();
            }
        }
        // Decimal number
        while (is_digit(peek())) advance();
        return make_number_token();
    }
    
    // Identifiers (labels, instructions, registers)
    // Requirements: 8.2, 8.3
    if (is_alpha(c)) {
        while (is_alnum(peek())) advance();
        
        // Special case: handle b.cond (conditional branch) as single token
        // If we have 'b' followed by '.', include the condition code
        if ((current_ - start_) == 1 && to_lower(start_[0]) == 'b' && peek() == '.') {
            advance(); // consume '.'
            while (is_alpha(peek())) advance(); // consume condition code
        }
        
        return make_identifier_token();
    }
    
    // String literals
    if (c == '"') {
        return make_string_token();
    }
    
    return make_error_token("Unexpected character");
}

/*
 * Peek at the next token without consuming it
 */
Token Lexer::peek_token() {
    // Save current state
    const char* saved_current = current_;
    const char* saved_start = start_;
    int saved_line = line_;
    
    // Get next token
    Token token = next_token();
    
    // Restore state
    current_ = saved_current;
    start_ = saved_start;
    line_ = saved_line;
    
    return token;
}

/*
 * Get string representation of token type
 */
const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::IDENTIFIER:   return "IDENTIFIER";
        case TokenType::NUMBER:       return "NUMBER";
        case TokenType::STRING:       return "STRING";
        case TokenType::COLON:        return "COLON";
        case TokenType::COMMA:        return "COMMA";
        case TokenType::HASH:         return "HASH";
        case TokenType::LBRACKET:     return "LBRACKET";
        case TokenType::RBRACKET:     return "RBRACKET";
        case TokenType::EXCLAIM:      return "EXCLAIM";
        case TokenType::DOT:          return "DOT";
        case TokenType::PLUS:         return "PLUS";
        case TokenType::MINUS:        return "MINUS";
        case TokenType::NEWLINE:      return "NEWLINE";
        case TokenType::END_OF_FILE:  return "END_OF_FILE";
        case TokenType::ERROR:        return "ERROR";
        default:                      return "UNKNOWN";
    }
}

/*
 * Check if an identifier is a valid ARM64 register name
 * Requirements: 8.3
 * 
 * Valid registers:
 * - x0-x30 (64-bit general purpose)
 * - w0-w30 (32-bit general purpose)
 * - sp (stack pointer)
 * - lr (link register, alias for x30)
 * - xzr (64-bit zero register)
 * - wzr (32-bit zero register)
 */
bool is_register_name(const char* name, size_t length) {
    if (length == 0) return false;
    
    // Check for sp, lr, xzr, wzr
    if (length == 2) {
        if ((to_lower(name[0]) == 's' && to_lower(name[1]) == 'p') ||
            (to_lower(name[0]) == 'l' && to_lower(name[1]) == 'r')) {
            return true;
        }
    }
    
    if (length == 3) {
        if ((to_lower(name[0]) == 'x' && to_lower(name[1]) == 'z' && to_lower(name[2]) == 'r') ||
            (to_lower(name[0]) == 'w' && to_lower(name[1]) == 'z' && to_lower(name[2]) == 'r')) {
            return true;
        }
    }
    
    // Check for x0-x30 or w0-w30
    if (length >= 2 && length <= 3) {
        char prefix = to_lower(name[0]);
        if (prefix == 'x' || prefix == 'w') {
            // Parse register number
            int num = 0;
            for (size_t i = 1; i < length; i++) {
                if (name[i] < '0' || name[i] > '9') return false;
                num = num * 10 + (name[i] - '0');
            }
            return num >= 0 && num <= 30;
        }
    }
    
    return false;
}

} // namespace casm
