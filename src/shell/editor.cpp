/*
 * EmberOS Vi-like Text Editor Implementation
 * Improved line-based text editor with vi-like commands
 */

#include "editor.h"
#include "uart.h"
#include "ramfs.h"

namespace editor {

// ============================================================================
// String Utilities
// ============================================================================

static size_t str_len(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static void str_copy(char* dst, const char* src, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int str_cmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

static void mem_move(void* dst, const void* src, size_t size) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    
    if (d < s) {
        for (size_t i = 0; i < size; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = size; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
}

// Print number without printf
static void print_num(int n) {
    if (n < 0) {
        uart::putc('-');
        n = -n;
    }
    if (n >= 10) {
        print_num(n / 10);
    }
    uart::putc('0' + (n % 10));
}

// Print number with padding
static void print_num_padded(int n, int width) {
    char buf[12];
    int len = 0;
    
    if (n == 0) {
        buf[len++] = '0';
    } else {
        while (n > 0) {
            buf[len++] = '0' + (n % 10);
            n /= 10;
        }
    }
    
    for (int i = len; i < width; i++) {
        uart::putc(' ');
    }
    
    while (len > 0) {
        uart::putc(buf[--len]);
    }
}

// ============================================================================
// Editor State
// ============================================================================

struct EditorState {
    char lines[MAX_LINES][MAX_LINE_LEN];
    size_t line_count;
    size_t cursor_line;
    size_t cursor_col;
    size_t scroll_offset;
    bool modified;
    char filename[ramfs::MAX_FILENAME];
    char status_msg[80];
    bool insert_mode;
};

static EditorState g_editor;

// Terminal dimensions
constexpr size_t TERM_ROWS = 24;
constexpr size_t TERM_COLS = 80;
constexpr size_t EDIT_ROWS = TERM_ROWS - 2;

// ============================================================================
// Terminal Control
// ============================================================================

static void clear_screen() {
    uart::puts("\x1b[2J\x1b[H");
}

static void move_cursor(size_t row, size_t col) {
    uart::puts("\x1b[");
    print_num((int)(row + 1));
    uart::putc(';');
    print_num((int)(col + 1));
    uart::putc('H');
}

static void clear_line() {
    uart::puts("\x1b[K");
}

static void set_reverse() {
    uart::puts("\x1b[7m");
}

static void reset_attr() {
    uart::puts("\x1b[0m");
}

static void hide_cursor() {
    uart::puts("\x1b[?25l");
}

static void show_cursor() {
    uart::puts("\x1b[?25h");
}

// ============================================================================
// Editor Operations
// ============================================================================

static void init_editor(const char* filename) {
    // Zero out state manually
    for (size_t i = 0; i < MAX_LINES; i++) {
        g_editor.lines[i][0] = '\0';
    }
    g_editor.line_count = 1;
    g_editor.cursor_line = 0;
    g_editor.cursor_col = 0;
    g_editor.scroll_offset = 0;
    g_editor.modified = false;
    g_editor.insert_mode = false;
    
    str_copy(g_editor.filename, filename, ramfs::MAX_FILENAME);
    str_copy(g_editor.status_msg, "NORMAL | hjkl:move i:insert :wq:save&quit :q!:quit", 80);
}

static void load_file() {
    ramfs::FSNode* file = ramfs::open_file(g_editor.filename);
    if (!file) {
        str_copy(g_editor.status_msg, "[New File] Press i to insert text", 80);
        return;
    }
    
    uint8_t buffer[ramfs::MAX_FILE_SIZE];
    size_t size = ramfs::read_file(file, buffer, 0, ramfs::MAX_FILE_SIZE);
    
    g_editor.line_count = 0;
    size_t line_pos = 0;
    
    for (size_t i = 0; i < size && g_editor.line_count < MAX_LINES; i++) {
        if (buffer[i] == '\n' || line_pos >= MAX_LINE_LEN - 1) {
            g_editor.lines[g_editor.line_count][line_pos] = '\0';
            g_editor.line_count++;
            line_pos = 0;
        } else if (buffer[i] != '\r') {
            g_editor.lines[g_editor.line_count][line_pos++] = buffer[i];
        }
    }
    
    if (line_pos > 0 || g_editor.line_count == 0) {
        g_editor.lines[g_editor.line_count][line_pos] = '\0';
        g_editor.line_count++;
    }
    
    if (g_editor.line_count == 0) {
        g_editor.line_count = 1;
        g_editor.lines[0][0] = '\0';
    }
}

static bool save_file() {
    ramfs::FSNode* file = ramfs::open_file(g_editor.filename);
    if (!file) {
        file = ramfs::create_file(g_editor.filename);
        if (!file) {
            str_copy(g_editor.status_msg, "Error: Cannot create file", 80);
            return false;
        }
    }
    
    ramfs::truncate_file(file, 0);
    
    size_t offset = 0;
    for (size_t i = 0; i < g_editor.line_count; i++) {
        size_t len = str_len(g_editor.lines[i]);
        if (len > 0) {
            ramfs::write_file(file, reinterpret_cast<const uint8_t*>(g_editor.lines[i]), 
                             offset, len);
            offset += len;
        }
        if (i < g_editor.line_count - 1) {
            uint8_t newline = '\n';
            ramfs::write_file(file, &newline, offset, 1);
            offset++;
        }
    }
    
    g_editor.modified = false;
    str_copy(g_editor.status_msg, "File saved!", 80);
    return true;
}

static void ensure_cursor_visible() {
    if (g_editor.cursor_line < g_editor.scroll_offset) {
        g_editor.scroll_offset = g_editor.cursor_line;
    }
    if (g_editor.cursor_line >= g_editor.scroll_offset + EDIT_ROWS) {
        g_editor.scroll_offset = g_editor.cursor_line - EDIT_ROWS + 1;
    }
    
    size_t line_len = str_len(g_editor.lines[g_editor.cursor_line]);
    if (g_editor.cursor_col > line_len) {
        g_editor.cursor_col = line_len;
    }
}

static void draw_screen() {
    hide_cursor();
    move_cursor(0, 0);
    
    for (size_t row = 0; row < EDIT_ROWS; row++) {
        size_t line_idx = g_editor.scroll_offset + row;
        
        if (line_idx < g_editor.line_count) {
            print_num_padded((int)(line_idx + 1), 3);
            uart::putc(' ');
            
            const char* line = g_editor.lines[line_idx];
            size_t len = str_len(line);
            size_t max_chars = TERM_COLS - 4;
            
            for (size_t i = 0; i < len && i < max_chars; i++) {
                uart::putc(line[i]);
            }
        } else {
            uart::puts("~");
        }
        
        clear_line();
        uart::puts("\r\n");
    }
    
    // Status bar
    set_reverse();
    
    if (g_editor.insert_mode) {
        uart::puts(" INSERT ");
    } else {
        uart::puts(" NORMAL ");
    }
    
    uart::puts("| ");
    uart::puts(g_editor.filename);
    if (g_editor.modified) {
        uart::puts(" [+]");
    }
    
    size_t used = 10 + str_len(g_editor.filename) + (g_editor.modified ? 4 : 0);
    
    char pos_info[20];
    int pos_len = 0;
    pos_info[pos_len++] = 'L';
    
    int ln = (int)(g_editor.cursor_line + 1);
    char tmp[8];
    int tlen = 0;
    do { tmp[tlen++] = '0' + (ln % 10); ln /= 10; } while (ln > 0);
    while (tlen > 0) pos_info[pos_len++] = tmp[--tlen];
    
    pos_info[pos_len++] = ':';
    pos_info[pos_len++] = 'C';
    
    int cn = (int)(g_editor.cursor_col + 1);
    tlen = 0;
    do { tmp[tlen++] = '0' + (cn % 10); cn /= 10; } while (cn > 0);
    while (tlen > 0) pos_info[pos_len++] = tmp[--tlen];
    
    pos_info[pos_len++] = ' ';
    pos_info[pos_len] = '\0';
    
    for (size_t i = used; i < TERM_COLS - pos_len; i++) {
        uart::putc(' ');
    }
    uart::puts(pos_info);
    
    reset_attr();
    uart::puts("\r\n");
    
    uart::puts(g_editor.status_msg);
    clear_line();
    
    size_t screen_row = g_editor.cursor_line - g_editor.scroll_offset;
    size_t screen_col = g_editor.cursor_col + 4;
    move_cursor(screen_row, screen_col);
    show_cursor();
}

static void cursor_up() {
    if (g_editor.cursor_line > 0) {
        g_editor.cursor_line--;
        ensure_cursor_visible();
    }
}

static void cursor_down() {
    if (g_editor.cursor_line < g_editor.line_count - 1) {
        g_editor.cursor_line++;
        ensure_cursor_visible();
    }
}

static void cursor_left() {
    if (g_editor.cursor_col > 0) {
        g_editor.cursor_col--;
    }
}

static void cursor_right() {
    size_t line_len = str_len(g_editor.lines[g_editor.cursor_line]);
    if (g_editor.cursor_col < line_len) {
        g_editor.cursor_col++;
    }
}

static void insert_char(char c) {
    char* line = g_editor.lines[g_editor.cursor_line];
    size_t len = str_len(line);
    
    if (len >= MAX_LINE_LEN - 1) return;
    
    mem_move(line + g_editor.cursor_col + 1, 
             line + g_editor.cursor_col, 
             len - g_editor.cursor_col + 1);
    
    line[g_editor.cursor_col] = c;
    g_editor.cursor_col++;
    g_editor.modified = true;
}

static void delete_char_back() {
    char* line = g_editor.lines[g_editor.cursor_line];
    size_t len = str_len(line);
    
    if (g_editor.cursor_col > 0) {
        mem_move(line + g_editor.cursor_col - 1,
                 line + g_editor.cursor_col,
                 len - g_editor.cursor_col + 1);
        g_editor.cursor_col--;
        g_editor.modified = true;
    } else if (g_editor.cursor_line > 0) {
        size_t prev_len = str_len(g_editor.lines[g_editor.cursor_line - 1]);
        
        if (prev_len + len < MAX_LINE_LEN - 1) {
            str_copy(g_editor.lines[g_editor.cursor_line - 1] + prev_len, 
                    line, MAX_LINE_LEN - prev_len);
            
            for (size_t i = g_editor.cursor_line; i < g_editor.line_count - 1; i++) {
                str_copy(g_editor.lines[i], g_editor.lines[i + 1], MAX_LINE_LEN);
            }
            g_editor.line_count--;
            
            g_editor.cursor_line--;
            g_editor.cursor_col = prev_len;
            g_editor.modified = true;
            ensure_cursor_visible();
        }
    }
}

static void insert_newline() {
    if (g_editor.line_count >= MAX_LINES) return;
    
    char* line = g_editor.lines[g_editor.cursor_line];
    
    for (size_t i = g_editor.line_count; i > g_editor.cursor_line + 1; i--) {
        str_copy(g_editor.lines[i], g_editor.lines[i - 1], MAX_LINE_LEN);
    }
    g_editor.line_count++;
    
    str_copy(g_editor.lines[g_editor.cursor_line + 1], 
            line + g_editor.cursor_col, MAX_LINE_LEN);
    line[g_editor.cursor_col] = '\0';
    
    g_editor.cursor_line++;
    g_editor.cursor_col = 0;
    g_editor.modified = true;
    ensure_cursor_visible();
}

static void delete_line() {
    if (g_editor.line_count <= 1) {
        g_editor.lines[0][0] = '\0';
        g_editor.cursor_col = 0;
        g_editor.modified = true;
        return;
    }
    
    for (size_t i = g_editor.cursor_line; i < g_editor.line_count - 1; i++) {
        str_copy(g_editor.lines[i], g_editor.lines[i + 1], MAX_LINE_LEN);
    }
    g_editor.line_count--;
    
    if (g_editor.cursor_line >= g_editor.line_count) {
        g_editor.cursor_line = g_editor.line_count - 1;
    }
    g_editor.cursor_col = 0;
    g_editor.modified = true;
    ensure_cursor_visible();
}

// Check if more input is available (non-blocking)
static bool input_available() {
    return uart::has_input();
}

// Process escape sequence (arrow keys)
static void process_escape() {
    // Wait briefly for escape sequence
    for (int i = 0; i < 1000 && !input_available(); i++) {
        // Small delay
        for (volatile int j = 0; j < 100; j++);
    }
    
    if (!input_available()) {
        // Just ESC key - exit insert mode
        if (g_editor.insert_mode) {
            g_editor.insert_mode = false;
            str_copy(g_editor.status_msg, "NORMAL | hjkl:move i:insert :wq:save&quit", 80);
            if (g_editor.cursor_col > 0) {
                g_editor.cursor_col--;
            }
        }
        return;
    }
    
    char c = uart::getc();
    if (c != '[') return;
    
    if (!input_available()) return;
    c = uart::getc();
    
    switch (c) {
        case 'A': cursor_up(); break;
        case 'B': cursor_down(); break;
        case 'C': cursor_right(); break;
        case 'D': cursor_left(); break;
        case 'H': g_editor.cursor_col = 0; break;
        case 'F': g_editor.cursor_col = str_len(g_editor.lines[g_editor.cursor_line]); break;
        case '3':
            if (input_available()) {
                c = uart::getc();
                if (c == '~') {
                    char* line = g_editor.lines[g_editor.cursor_line];
                    size_t len = str_len(line);
                    if (g_editor.cursor_col < len) {
                        mem_move(line + g_editor.cursor_col,
                                 line + g_editor.cursor_col + 1,
                                 len - g_editor.cursor_col);
                        g_editor.modified = true;
                    }
                }
            }
            break;
    }
}

// Process command (after :)
static Result process_command() {
    char cmd[32];
    size_t cmd_len = 0;
    
    move_cursor(TERM_ROWS - 1, 0);
    clear_line();
    uart::putc(':');
    
    while (true) {
        char c = uart::getc();
        
        if (c == '\r' || c == '\n') {
            cmd[cmd_len] = '\0';
            break;
        } else if (c == '\x1b') {
            str_copy(g_editor.status_msg, "Command cancelled", 80);
            return Result::ERROR;
        } else if (c == '\x7f' || c == '\b') {
            if (cmd_len > 0) {
                cmd_len--;
                uart::puts("\b \b");
            }
        } else if (cmd_len < sizeof(cmd) - 1 && c >= 0x20) {
            cmd[cmd_len++] = c;
            uart::putc(c);
        }
    }
    
    if (str_cmp(cmd, "q") == 0) {
        if (g_editor.modified) {
            str_copy(g_editor.status_msg, "Unsaved changes! :q! to force, :wq to save", 80);
            return Result::ERROR;
        }
        return Result::QUIT;
    } else if (str_cmp(cmd, "q!") == 0) {
        return Result::QUIT;
    } else if (str_cmp(cmd, "w") == 0) {
        save_file();
        return Result::ERROR;
    } else if (str_cmp(cmd, "wq") == 0 || str_cmp(cmd, "x") == 0) {
        if (save_file()) {
            return Result::SAVED;
        }
        return Result::ERROR;
    } else {
        str_copy(g_editor.status_msg, "Unknown command. Try :w :q :wq :q!", 80);
        return Result::ERROR;
    }
}

Result edit(const char* filename) {
    init_editor(filename);
    load_file();
    
    clear_screen();
    
    while (true) {
        draw_screen();
        
        char c = uart::getc();
        
        if (g_editor.insert_mode) {
            // INSERT MODE
            if (c == '\x1b') {
                process_escape();
            } else if (c == '\r' || c == '\n') {
                insert_newline();
            } else if (c == '\x7f' || c == '\b') {
                delete_char_back();
            } else if (c == '\t') {
                // Insert 4 spaces for tab
                for (int i = 0; i < 4; i++) {
                    insert_char(' ');
                }
            } else if (c >= 0x20 && c < 0x7f) {
                insert_char(c);
            }
        } else {
            // NORMAL MODE
            switch (c) {
                case 'h': cursor_left(); break;
                case 'j': cursor_down(); break;
                case 'k': cursor_up(); break;
                case 'l': cursor_right(); break;
                
                case '\x1b':
                    process_escape();
                    break;
                
                case 'i':
                    g_editor.insert_mode = true;
                    str_copy(g_editor.status_msg, "INSERT | ESC:normal mode", 80);
                    break;
                
                case 'a':
                    g_editor.insert_mode = true;
                    str_copy(g_editor.status_msg, "INSERT | ESC:normal mode", 80);
                    cursor_right();
                    break;
                
                case 'A':
                    g_editor.cursor_col = str_len(g_editor.lines[g_editor.cursor_line]);
                    g_editor.insert_mode = true;
                    str_copy(g_editor.status_msg, "INSERT | ESC:normal mode", 80);
                    break;
                
                case 'o':
                    g_editor.cursor_col = str_len(g_editor.lines[g_editor.cursor_line]);
                    insert_newline();
                    g_editor.insert_mode = true;
                    str_copy(g_editor.status_msg, "INSERT | ESC:normal mode", 80);
                    break;
                
                case 'O':
                    if (g_editor.line_count < MAX_LINES) {
                        for (size_t i = g_editor.line_count; i > g_editor.cursor_line; i--) {
                            str_copy(g_editor.lines[i], g_editor.lines[i - 1], MAX_LINE_LEN);
                        }
                        g_editor.lines[g_editor.cursor_line][0] = '\0';
                        g_editor.line_count++;
                        g_editor.cursor_col = 0;
                        g_editor.modified = true;
                        g_editor.insert_mode = true;
                        str_copy(g_editor.status_msg, "INSERT | ESC:normal mode", 80);
                    }
                    break;
                
                case 'x':
                    {
                        char* line = g_editor.lines[g_editor.cursor_line];
                        size_t len = str_len(line);
                        if (len > 0 && g_editor.cursor_col < len) {
                            mem_move(line + g_editor.cursor_col,
                                     line + g_editor.cursor_col + 1,
                                     len - g_editor.cursor_col);
                            g_editor.modified = true;
                            if (g_editor.cursor_col >= str_len(line) && g_editor.cursor_col > 0) {
                                g_editor.cursor_col--;
                            }
                        }
                    }
                    break;
                
                case 'd':
                    c = uart::getc();
                    if (c == 'd') {
                        delete_line();
                    }
                    break;
                
                case 'G':
                    g_editor.cursor_line = g_editor.line_count - 1;
                    g_editor.cursor_col = 0;
                    ensure_cursor_visible();
                    break;
                
                case 'g':
                    c = uart::getc();
                    if (c == 'g') {
                        g_editor.cursor_line = 0;
                        g_editor.cursor_col = 0;
                        ensure_cursor_visible();
                    }
                    break;
                
                case '0':
                    g_editor.cursor_col = 0;
                    break;
                
                case '$':
                    {
                        size_t len = str_len(g_editor.lines[g_editor.cursor_line]);
                        g_editor.cursor_col = len > 0 ? len - 1 : 0;
                    }
                    break;
                
                case ':':
                    {
                        Result result = process_command();
                        if (result == Result::SAVED || result == Result::QUIT) {
                            clear_screen();
                            return result;
                        }
                    }
                    break;
            }
        }
    }
}

} // namespace editor
