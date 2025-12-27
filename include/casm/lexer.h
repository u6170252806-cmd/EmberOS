/*
 * EmberOS CASM Lexer Header
 * Tokenizer for C.ASM assembly language
 * 
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5
 */

#ifndef EMBEROS_CASM_LEXER_H
#define EMBEROS_CASM_LEXER_H

// Freestanding type definitions
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using int64_t = long long;
using size_t = unsigned long;
using uintptr_t = unsigned long;

namespace casm {

/*
 * Token types recognized by the CASM lexer
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5
 */
enum class TokenType {
    // Identifiers and literals
    IDENTIFIER,     // Labels, instruction names, register names
    NUMBER,         // Numeric literals (decimal, hex, binary)
    STRING,         // String literals (for directives)
    
    // Punctuation
    COLON,          // : (label definition)
    COMMA,          // , (operand separator)
    HASH,           // # (immediate value prefix)
    LBRACKET,       // [ (memory operand start)
    RBRACKET,       // ] (memory operand end)
    EXCLAIM,        // ! (pre-index indicator)
    DOT,            // . (directive prefix)
    PLUS,           // + (offset addition)
    MINUS,          // - (offset subtraction / negative)
    
    // Special
    NEWLINE,        // End of line
    END_OF_FILE,    // End of input
    ERROR           // Lexical error
};

/*
 * Token structure containing type, position, and value
 */
struct Token {
    TokenType type;
    const char* start;      // Pointer to start of token in source
    size_t length;          // Length of token text
    int line;               // Line number (1-based)
    int64_t number_value;   // Numeric value for NUMBER tokens
};

/*
 * Lexer class for tokenizing C.ASM source code
 * 
 * Usage:
 *   Lexer lexer(source_code);
 *   Token tok = lexer.next_token();
 *   while (tok.type != TokenType::END_OF_FILE) {
 *       // process token
 *       tok = lexer.next_token();
 *   }
 */
class Lexer {
public:
    /*
     * Construct a lexer for the given source code
     * source: null-terminated C.ASM source string
     */
    explicit Lexer(const char* source);
    
    /*
     * Get the next token from the source
     * Advances the lexer position
     * Returns ERROR token on lexical errors
     */
    Token next_token();
    
    /*
     * Peek at the next token without consuming it
     * Does not advance the lexer position
     */
    Token peek_token();
    
    /*
     * Get current line number (1-based)
     */
    int get_line() const { return line_; }
    
    /*
     * Check if lexer has reached end of input
     */
    bool is_at_end() const { return *current_ == '\0'; }

private:
    const char* source_;    // Original source string
    const char* current_;   // Current position in source
    const char* start_;     // Start of current token
    int line_;              // Current line number
    
    // Internal helper methods
    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);
    void skip_whitespace();
    void skip_line_comment();
    
    Token make_token(TokenType type);
    Token make_error_token(const char* message);
    Token make_number_token();
    Token make_identifier_token();
    Token make_string_token();
    
    bool is_digit(char c) const;
    bool is_alpha(char c) const;
    bool is_alnum(char c) const;
    bool is_hex_digit(char c) const;
    bool is_binary_digit(char c) const;
    
    int64_t parse_decimal();
    int64_t parse_hexadecimal();
    int64_t parse_binary();
};

/*
 * Helper function to get string representation of token type
 * Useful for debugging and error messages
 */
const char* token_type_to_string(TokenType type);

/*
 * Check if an identifier is a valid ARM64 register name
 * Returns true for: x0-x30, w0-w30, sp, lr, xzr, wzr
 * Requirements: 8.3
 */
bool is_register_name(const char* name, size_t length);

/*
 * CASM Extended Opcodes for I/O and control
 * These are pseudo-instructions that get encoded as SVC calls
 * with specific immediate values for the VM to interpret
 */
enum class ExtendedOpcode : uint16_t {
    // I/O opcodes (0x100-0x10F)
    PRT   = 0x100,   // Print null-terminated string at address in x0
    PRTC  = 0x101,   // Print character in w0
    PRTN  = 0x102,   // Print number in x0
    INP   = 0x103,   // Input character, result in w0
    
    // Graphics opcodes (0x110-0x11F)
    CLS   = 0x110,   // Clear canvas area (not whole screen)
    SETC  = 0x111,   // Set color (fg in w0, bg in w1)
    PLOT  = 0x112,   // Plot character at x0,y0 (char in w2) - relative to canvas
    LINE  = 0x113,   // Draw line from x0,y0 to x1,y1 (char in w2)
    BOX   = 0x114,   // Draw box at x0,y0 size x1,y1
    RESET = 0x115,   // Reset terminal colors
    CANVAS= 0x116,   // Set canvas size (w0=width, w1=height) - creates drawing area
    
    // System opcodes (0x1F0-0x1FF)
    SLEEP = 0x1F0,   // Sleep for x0 milliseconds
    RND   = 0x1F1,   // Random number (0 to x0-1), result in x0
    TICK  = 0x1F2,   // Get current time in ms, result in x0
    HALT  = 0x1FF    // Halt execution
};

} // namespace casm

#endif // EMBEROS_CASM_LEXER_H
