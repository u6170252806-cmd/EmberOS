# C.ASM Programming Guide

Complete documentation for the C.ASM assembly language and CASM compiler.

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [The CASM Compiler](#the-casm-compiler)
4. [Language Syntax](#language-syntax)
5. [Registers](#registers)
6. [Instructions Reference](#instructions-reference)
7. [Extended Opcodes](#extended-opcodes)
8. [Memory Model](#memory-model)
9. [Common Patterns](#common-patterns)
10. [Debugging](#debugging)
11. [Pitfalls & Gotchas](#pitfalls--gotchas)

---

## Introduction

C.ASM (C-Assembly) is a simplified ARM64 assembly dialect designed for EmberOS. Unlike traditional assembly that runs directly on hardware, C.ASM programs run in a sandboxed virtual machine interpreter.

### Why C.ASM?

- **Safe execution** - Programs can't crash the OS
- **Built-in I/O** - No complex syscall setup needed
- **Instant graphics** - Draw to screen with simple opcodes
- **File access** - Read/write files directly
- **Learning friendly** - Simpler than raw ARM64

### What C.ASM is NOT

- Not a high-level language (no variables, functions, or types)
- Not native execution (interpreted, so slower than native)
- Not full ARM64 (subset of instructions supported)


---

## Architecture Overview

### Execution Flow

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│ Source File │ --> │    CASM     │ --> │   Binary    │
│  (.asm)     │     │  Compiler   │     │   (.bin)    │
└─────────────┘     └─────────────┘     └─────────────┘
                                              │
                                              v
                                        ┌─────────────┐
                                        │  CASM VM    │
                                        │ Interpreter │
                                        └─────────────┘
```

### Two-Pass Assembly

The CASM compiler uses two passes:

1. **Pass 1**: Scan for labels, calculate addresses
2. **Pass 2**: Generate machine code, resolve label references

This allows forward references (jumping to labels defined later).

### The Virtual Machine

The VM interprets ARM64 instructions with these components:

- **32 registers** (x0-x31, with x31 as zero register)
- **Condition flags** (N, Z, C, V for comparisons)
- **5KB memory** (code + data combined)
- **Program counter** (tracks current instruction)
- **Virtual framebuffer** (for graphics)

---

## The CASM Compiler

### Basic Usage

```bash
# Compile to binary
casm source.asm -o output.bin

# Compile and run immediately
casm -r source.asm

# Run existing binary
casm run program.bin

# Debug mode (step through)
casm run -d program.bin

# Disassemble binary
casm disasm program.bin
```

### Compiler Output

When compiling, CASM shows:
- Code size in bytes
- Symbol table (labels and addresses)
- Generated machine code (hex dump)

Example:
```
Assembling 'hello.asm'...

Assembly successful!
  Code size: 24 bytes
  Symbols:   1

Symbol table:
  _start = 0x0

Generated code:
  0000: 52800900 d4002021 52800540 d4002021
  0010: 52800140 d4002021 d4003fe1
```

### Error Messages

Common errors:
- `Parse error at line N` - Syntax error
- `Undefined symbol` - Label not found
- `Code buffer overflow` - Program too large (>5KB)
- `Invalid register` - Wrong register name
- `Unknown instruction` - Unsupported opcode


---

## Language Syntax

### Basic Structure

```asm
; This is a comment (semicolon to end of line)

.text                   ; Code section directive
_start:                 ; Label (entry point)
    mov x0, #42         ; Instruction with immediate
    add x1, x0, #1      ; Three-operand instruction
    halt                ; Extended opcode
```

### Labels

Labels mark addresses in code. They must:
- Start with letter or underscore
- Contain only letters, numbers, underscores
- End with colon when defined

```asm
_start:          ; Definition (note the colon)
    b loop       ; Reference (no colon)
    
loop:            ; Another label
    b _start     ; Jump back
```

### Directives

```asm
.text            ; Switch to code section
.data            ; Switch to data section (not commonly used)
.global _start   ; Mark symbol as global
.align 2         ; Align to 4-byte boundary (2^2)
.byte 0x41       ; Emit single byte
.word 0x12345678 ; Emit 32-bit word
.asciz "Hello"   ; Emit null-terminated string
.space 16        ; Reserve 16 bytes of zeros
```

### Numbers

```asm
mov x0, #42      ; Decimal
mov x0, #0x2A    ; Hexadecimal
mov x0, #0b101   ; Binary (if supported)
```

**Important**: Character literals like `#'A'` are NOT supported. Use ASCII codes:
```asm
mov w0, #65      ; 'A' = 65
mov w0, #10      ; newline = 10
mov w0, #32      ; space = 32
```

### Comments

```asm
; Full line comment
mov x0, #1       ; End of line comment
```

---

## Registers

### General Purpose Registers

| Register | Size | Description |
|----------|------|-------------|
| `x0-x30` | 64-bit | General purpose |
| `w0-w30` | 32-bit | Lower 32 bits of x registers |
| `x31/xzr` | 64-bit | Zero register (always reads 0) |
| `w31/wzr` | 32-bit | 32-bit zero register |
| `sp` | 64-bit | Stack pointer (alias for x31 in some contexts) |
| `lr` | 64-bit | Link register (alias for x30) |

### When to Use W vs X

- Use `w` registers for:
  - Characters (8-bit values stored in 32-bit)
  - Small counters
  - Boolean flags

- Use `x` registers for:
  - Memory addresses
  - Large numbers
  - 64-bit arithmetic

```asm
mov w0, #65      ; Character 'A' - use w
mov x1, #0x500   ; Memory address - use x
```

### Register Conventions

| Register | Convention |
|----------|------------|
| `x0` | Primary argument/return value |
| `x1-x7` | Additional arguments |
| `x8-x15` | Temporary (caller-saved) |
| `x16-x29` | Saved (callee-saved) |
| `x30/lr` | Return address |

For C.ASM extended opcodes:
- `x0` - Primary input/output
- `x1, x2, x3...` - Additional parameters


---

## Instructions Reference

### Data Movement

#### MOV - Move
```asm
mov x0, #123         ; Load immediate value
mov x0, x1           ; Copy register to register
mov w0, #65          ; 32-bit immediate
```

The assembler chooses the right encoding:
- Small immediates: `MOVZ`
- Register moves: `ORR` with zero register

#### MOVZ/MOVN/MOVK - Move variants
```asm
movz x0, #0x1234     ; Move with zero (clear other bits)
movn x0, #0          ; Move NOT (for -1)
movk x0, #0x5678, lsl #16  ; Move keep (modify part)
```

### Arithmetic

#### ADD - Addition
```asm
add x0, x1, #10      ; x0 = x1 + 10
add x0, x1, x2       ; x0 = x1 + x2
add w0, w1, #5       ; 32-bit addition
```

#### SUB - Subtraction
```asm
sub x0, x1, #10      ; x0 = x1 - 10
sub x0, x1, x2       ; x0 = x1 - x2
```

#### MUL - Multiplication
```asm
mul x0, x1, x2       ; x0 = x1 * x2
```

**Note**: Division (`udiv`, `sdiv`) may not be fully supported.

### Comparison

#### CMP - Compare
```asm
cmp x0, #10          ; Compare x0 with immediate
cmp x0, x1           ; Compare two registers
```

CMP sets condition flags but doesn't store result. It's actually `SUBS` with destination as zero register.

#### Condition Flags

| Flag | Meaning |
|------|---------|
| N | Negative (bit 63/31 of result) |
| Z | Zero (result was zero) |
| C | Carry (unsigned overflow) |
| V | Overflow (signed overflow) |

### Branching

#### Unconditional Branch
```asm
b label              ; Jump to label
bl function          ; Branch with link (call)
ret                  ; Return (branch to lr)
```

#### Conditional Branch
```asm
b.eq label           ; Branch if equal (Z=1)
b.ne label           ; Branch if not equal (Z=0)
b.lt label           ; Branch if less than (signed)
b.gt label           ; Branch if greater than (signed)
b.le label           ; Branch if less or equal
b.ge label           ; Branch if greater or equal
b.lo label           ; Branch if lower (unsigned)
b.hi label           ; Branch if higher (unsigned)
b.ls label           ; Branch if lower or same
b.hs label           ; Branch if higher or same
```

#### Condition Codes Summary

| Code | Meaning | Flags |
|------|---------|-------|
| EQ | Equal | Z=1 |
| NE | Not equal | Z=0 |
| LT | Less than (signed) | N≠V |
| GT | Greater than (signed) | Z=0 AND N=V |
| LE | Less or equal (signed) | Z=1 OR N≠V |
| GE | Greater or equal (signed) | N=V |
| LO/CC | Lower (unsigned) | C=0 |
| HI | Higher (unsigned) | C=1 AND Z=0 |
| LS | Lower or same (unsigned) | C=0 OR Z=1 |
| HS/CS | Higher or same (unsigned) | C=1 |

### Memory Access

#### Store Byte
```asm
strb w0, [x1]        ; Store byte at address in x1
strb w0, [x1, #5]    ; Store at x1 + 5
```

#### Load Byte
```asm
ldrb w0, [x1]        ; Load byte from address in x1
ldrb w0, [x1, #5]    ; Load from x1 + 5
```

#### Store/Load 64-bit
```asm
str x0, [x1]         ; Store 64-bit value
ldr x0, [x1]         ; Load 64-bit value
```

**Important**: Memory addresses must be within the 5KB program space.


---

## Extended Opcodes

Extended opcodes are C.ASM-specific instructions that provide high-level functionality. They're encoded as `SVC` instructions with special immediate values.

### I/O Operations

#### prt - Print String
Prints null-terminated string at address in x0.
```asm
mov x0, #0x500       ; Address of string
prt                  ; Print it
```

#### prtc - Print Character
Prints single character from w0.
```asm
mov w0, #65          ; 'A'
prtc                 ; Prints: A
```

#### prtn - Print Number
Prints x0 as decimal number.
```asm
mov x0, #42
prtn                 ; Prints: 42
```

#### prtx - Print Hex
Prints x0 as hexadecimal.
```asm
mov x0, #255
prtx                 ; Prints: 0xff
```

#### inp - Input Character
Waits for keypress, stores ASCII in x0.
```asm
inp                  ; Wait for key
; x0 now contains the character code
```

#### inps - Input String
Reads line of text into buffer.
```asm
mov x0, #0x500       ; Buffer address
mov x1, #64          ; Max length
inps                 ; Read input
; x0 = actual length read
```

### Graphics Operations

Graphics use a virtual framebuffer. On first graphics opcode, a 40x12 canvas is auto-created.

#### canvas - Set Canvas Size
```asm
mov x0, #60          ; Width (max 80)
mov x1, #20          ; Height (max 24)
canvas
```

#### cls - Clear Screen
```asm
cls                  ; Clear with current background
```

#### setc - Set Colors
```asm
mov x0, #2           ; Foreground: green
mov x1, #0           ; Background: black
setc
```

Color values: 0=black, 1=red, 2=green, 3=yellow, 4=blue, 5=magenta, 6=cyan, 7=white

#### plot - Draw Character
```asm
mov x0, #10          ; X position
mov x1, #5           ; Y position
mov x2, #42          ; Character '*'
plot
```

#### line - Draw Line
```asm
mov x0, #0           ; Start X
mov x1, #0           ; Start Y
mov x2, #20          ; End X
mov x3, #0           ; End Y
mov x4, #45          ; Character '-'
line
```

#### box - Draw Box
```asm
mov x0, #5           ; X position
mov x1, #2           ; Y position
mov x2, #20          ; Width
mov x3, #8           ; Height
box
```

Draws box with `+`, `-`, `|` characters.

#### reset - Reset Colors
```asm
reset                ; Back to white on black
```

### File Operations

File opcodes work with null-terminated filename strings in memory.

#### fcreat - Create File
```asm
mov x0, #0x500       ; Pointer to filename
fcreat
; x0 = 1 if success, 0 if failed
```

#### fwrite - Write File
```asm
mov x0, #0x500       ; Filename pointer
mov x1, #0x520       ; Data pointer
mov x2, #100         ; Length in bytes
fwrite
; x0 = bytes written
```

#### fread - Read File
```asm
mov x0, #0x500       ; Filename pointer
mov x1, #0x600       ; Buffer pointer
mov x2, #256         ; Max bytes to read
fread
; x0 = bytes read
```

#### fdel - Delete File
```asm
mov x0, #0x500       ; Filename pointer
fdel
; x0 = 1 if success
```

#### fcopy - Copy File
```asm
mov x0, #0x500       ; Source filename
mov x1, #0x540       ; Destination filename
fcopy
; x0 = 1 if success
```

#### fmove - Move/Rename File
```asm
mov x0, #0x500       ; Source filename
mov x1, #0x540       ; Destination filename
fmove
; x0 = 1 if success
```

#### fexist - Check File Exists
```asm
mov x0, #0x500       ; Filename pointer
fexist
; x0 = 1 if exists, 0 if not
```

### Memory/String Operations

#### strlen - String Length
```asm
mov x0, #0x500       ; String address
strlen
; x0 = length (not including null)
```

#### memcpy - Copy Memory
```asm
mov x0, #0x600       ; Destination
mov x1, #0x500       ; Source
mov x2, #100         ; Byte count
memcpy
```

#### memset - Fill Memory
```asm
mov x0, #0x500       ; Address
mov x1, #0           ; Byte value
mov x2, #100         ; Count
memset
```

#### abs - Absolute Value
```asm
mov x0, #0
sub x0, x0, #25      ; x0 = -25
abs
; x0 = 25
```

### System Operations

#### sleep - Delay
```asm
mov x0, #1000        ; Milliseconds
sleep                ; Wait 1 second
```

#### rnd - Random Number
```asm
mov x0, #100         ; Maximum (exclusive)
rnd
; x0 = random number 0-99
```

#### tick - Get Time
```asm
tick
; x0 = milliseconds since boot
```

#### halt - Stop Execution
```asm
halt                 ; End program
```


---

## Memory Model

### Layout

```
┌─────────────────────────────────────┐ 0x0000
│                                     │
│           Program Code              │
│                                     │
├─────────────────────────────────────┤ (varies)
│                                     │
│         Available for Data          │
│                                     │
├─────────────────────────────────────┤ 0x0500 (recommended data start)
│                                     │
│         Data Buffers                │
│         (strings, file data)        │
│                                     │
└─────────────────────────────────────┘ 0x1400 (5KB limit)
```

### Memory Regions

| Address | Usage |
|---------|-------|
| 0x0000 - 0x04FF | Code (up to ~1.25KB typical) |
| 0x0500 - 0x0FFF | General data buffers |
| 0x1000 - 0x13FF | Additional space |

### Best Practices

1. **Start data at 0x500** - Leaves room for code
2. **Use fixed addresses** - No dynamic allocation
3. **Plan buffer sizes** - Know your limits
4. **Null-terminate strings** - Required for prt, strlen

### Example Memory Usage

```asm
; Memory map for a program:
; 0x500-0x53F: filename buffer (64 bytes)
; 0x540-0x5FF: input buffer (192 bytes)
; 0x600-0x7FF: file content buffer (512 bytes)

.text
_start:
    mov x10, #0x500      ; filename buffer
    mov x11, #0x540      ; input buffer
    mov x12, #0x600      ; file buffer
    ; ... use these addresses
```

---

## Common Patterns

### Building Strings in Memory

Since character literals aren't supported, build strings byte by byte:

```asm
; Build "Hi" at 0x500
    mov x10, #0x500
    mov w0, #72          ; 'H'
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #105         ; 'i'
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #0           ; null terminator
    strb w0, [x10]
```

### Printing Multiple Characters

```asm
; Print "OK\n"
    mov w0, #79          ; 'O'
    prtc
    mov w0, #75          ; 'K'
    prtc
    mov w0, #10          ; newline
    prtc
```

### Counter Loop

```asm
    mov x10, #0          ; counter = 0
    mov x11, #10         ; limit = 10
loop:
    ; ... do something with x10 ...
    add x10, x10, #1     ; counter++
    cmp x10, x11         ; compare with limit
    b.lt loop            ; if counter < limit, continue
```

### Countdown Loop

```asm
    mov x10, #10         ; counter = 10
loop:
    ; ... do something ...
    sub x10, x10, #1     ; counter--
    cmp x10, #0
    b.gt loop            ; if counter > 0, continue
```

### Conditional Execution

```asm
    cmp x0, #0
    b.eq is_zero
    ; x0 is not zero
    b done
is_zero:
    ; x0 is zero
done:
```

### Function-like Pattern

```asm
_start:
    bl print_hello       ; call function
    bl print_hello       ; call again
    halt

print_hello:
    mov w0, #72          ; 'H'
    prtc
    mov w0, #105         ; 'i'
    prtc
    mov w0, #10
    prtc
    ret                  ; return
```

### Reading User Input

```asm
    ; Print prompt
    mov w0, #63          ; '?'
    prtc
    mov w0, #32          ; space
    prtc
    
    ; Read input
    mov x0, #0x500       ; buffer
    mov x1, #32          ; max length
    inps
    mov x8, x0           ; save length
    
    ; Echo back
    mov x0, #0x500
    prt
```

### Simple Menu

```asm
menu:
    ; Print options
    mov w0, #49          ; '1'
    prtc
    mov w0, #46          ; '.'
    prtc
    ; ... print option text ...
    
    ; Get choice
    inp
    
    cmp w0, #49          ; '1'?
    b.eq option1
    cmp w0, #50          ; '2'?
    b.eq option2
    b menu               ; invalid, retry

option1:
    ; handle option 1
    b menu

option2:
    ; handle option 2
    b menu
```


---

## Debugging

### Debug Mode

Run with `-d` flag to step through:

```
ember:/> casm run -d program.bin
Debug mode: Press ENTER to step, 'r' to run, 'q' to quit

PC=0000: 52800900  mov w0, #72
  x0-x3: 00000000 00000000 00000000 00000000
```

### Disassembly

View compiled code:

```
ember:/> casm disasm program.bin
Disassembly of 'program.bin' (24 bytes):

0000:  52800900  mov w0, #72
0004:  d4002021  prtc
0008:  52800540  mov w0, #42
...
```

### Debugging Tips

1. **Add print statements** - Use `prtn` to show values
2. **Check addresses** - Print pointers with `prtx`
3. **Verify strings** - Use `strlen` to check length
4. **Step through** - Use debug mode for complex logic

### Common Debug Pattern

```asm
    ; Debug: print register value
    mov x8, x0           ; save x0
    mov w0, #68          ; 'D'
    prtc
    mov w0, #58          ; ':'
    prtc
    mov x0, x8           ; restore for print
    prtn
    mov w0, #10
    prtc
    mov x0, x8           ; restore x0
```

---

## Pitfalls & Gotchas

### 1. No Character Literals

**Wrong:**
```asm
mov w0, #'A'         ; ERROR: Unexpected character
```

**Right:**
```asm
mov w0, #65          ; ASCII code for 'A'
```

### 2. Forgetting Null Terminators

**Wrong:**
```asm
mov x10, #0x500
mov w0, #72
strb w0, [x10]
mov x0, #0x500
prt                  ; May print garbage!
```

**Right:**
```asm
mov x10, #0x500
mov w0, #72
strb w0, [x10]
add x10, x10, #1
mov w0, #0           ; Null terminator!
strb w0, [x10]
mov x0, #0x500
prt
```

### 3. Using Wrong Register Size

**Problematic:**
```asm
mov x0, #65
prtc                 ; Works, but wasteful
```

**Better:**
```asm
mov w0, #65          ; Use w for small values
prtc
```

### 4. Forgetting to Save Registers

**Wrong:**
```asm
    mov x0, #42
    ; ... x0 gets overwritten by some opcode ...
    prtn               ; Prints wrong value!
```

**Right:**
```asm
    mov x0, #42
    mov x8, x0         ; Save to x8
    ; ... do other stuff ...
    mov x0, x8         ; Restore
    prtn
```

### 5. Branch Target Errors

**Wrong:**
```asm
    b.eq label         ; Forward reference
    ; ... code ...
lable:                 ; Typo! Different name
```

**Right:**
```asm
    b.eq label
    ; ... code ...
label:                 ; Matches exactly
```

### 6. Memory Address Confusion

**Wrong:**
```asm
mov x0, #0x500
mov w1, #65
strb w1, [x0]        ; OK
add x0, x0, #1
strb w1, [x0]        ; OK
; Later...
mov x0, #0x500       ; Forgot to reset!
prt                  ; Prints from wrong address
```

### 7. Infinite Loops

**Wrong:**
```asm
loop:
    ; forgot to increment counter
    cmp x10, #10
    b.lt loop          ; Never exits!
```

**Right:**
```asm
loop:
    add x10, x10, #1   ; Increment!
    cmp x10, #10
    b.lt loop
```

### 8. Off-by-One in Loops

**Prints 0-9 (10 numbers):**
```asm
    mov x10, #0
loop:
    mov x0, x10
    prtn
    add x10, x10, #1
    cmp x10, #10
    b.lt loop
```

**Prints 1-10 (10 numbers):**
```asm
    mov x10, #1
loop:
    mov x0, x10
    prtn
    add x10, x10, #1
    cmp x10, #11       ; Note: 11, not 10
    b.lt loop
```

### 9. File Operations Without Null Terminator

Filenames MUST be null-terminated:

```asm
; Build "test" filename
mov x10, #0x500
mov w0, #116         ; 't'
strb w0, [x10]
add x10, x10, #1
mov w0, #101         ; 'e'
strb w0, [x10]
add x10, x10, #1
mov w0, #115         ; 's'
strb w0, [x10]
add x10, x10, #1
mov w0, #116         ; 't'
strb w0, [x10]
add x10, x10, #1
mov w0, #0           ; NULL - Don't forget!
strb w0, [x10]
```

### 10. Graphics Not Rendering

Graphics only render when program ends. Don't expect real-time updates:

```asm
    plot               ; Draws to buffer
    sleep              ; Wait...
    plot               ; More drawing...
    halt               ; NOW it renders!
```

---

## ASCII Quick Reference

| Char | Code | Char | Code | Char | Code |
|------|------|------|------|------|------|
| space | 32 | 0 | 48 | A | 65 |
| ! | 33 | 1 | 49 | B | 66 |
| " | 34 | 2 | 50 | ... | ... |
| # | 35 | ... | ... | Z | 90 |
| $ | 36 | 9 | 57 | a | 97 |
| % | 37 | : | 58 | b | 98 |
| & | 38 | ; | 59 | ... | ... |
| ' | 39 | < | 60 | z | 122 |
| ( | 40 | = | 61 | { | 123 |
| ) | 41 | > | 62 | \| | 124 |
| * | 42 | ? | 63 | } | 125 |
| + | 43 | @ | 64 | ~ | 126 |
| , | 44 | | | | |
| - | 45 | newline | 10 | |
| . | 46 | tab | 9 | |
| / | 47 | | | |

---

## Example: Complete Program

Here's a well-structured program demonstrating best practices:

```asm
; countdown.asm - Countdown timer demo
; Demonstrates: loops, sleep, prtn, prtc

.text
_start:
    ; Print "Countdown:"
    mov w0, #67          ; 'C'
    prtc
    mov w0, #111         ; 'o'
    prtc
    mov w0, #117         ; 'u'
    prtc
    mov w0, #110         ; 'n'
    prtc
    mov w0, #116         ; 't'
    prtc
    mov w0, #58          ; ':'
    prtc
    mov w0, #10          ; newline
    prtc
    
    ; Initialize counter
    mov x10, #10         ; Start at 10
    
countdown_loop:
    ; Print current number
    mov x0, x10
    prtn
    mov w0, #10          ; newline
    prtc
    
    ; Wait 1 second
    mov x0, #1000
    sleep
    
    ; Decrement and check
    sub x10, x10, #1
    cmp x10, #0
    b.gt countdown_loop
    
    ; Print "Done!"
    mov w0, #68          ; 'D'
    prtc
    mov w0, #111         ; 'o'
    prtc
    mov w0, #110         ; 'n'
    prtc
    mov w0, #101         ; 'e'
    prtc
    mov w0, #33          ; '!'
    prtc
    mov w0, #10
    prtc
    
    halt
```

---

## Further Resources

- Check `examples/` directory for working programs
- Use `casm disasm` to study how code compiles
- Experiment in debug mode with `casm run -d`
- Start simple, add complexity gradually
