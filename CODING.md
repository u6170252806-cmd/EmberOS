# C.ASM Programming Guide

Complete documentation for the C.ASM assembly language and CASM compiler.

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [The CASM Compiler](#the-casm-compiler)
4. [Language Syntax](#language-syntax)
5. [Tutorial: Your First Programs](#tutorial-your-first-programs)
6. [Registers](#registers)
7. [Instructions Reference](#instructions-reference)
8. [Extended Opcodes](#extended-opcodes)
9. [Memory Model](#memory-model)
10. [Common Patterns](#common-patterns)
11. [Performance Optimizations](#performance-optimizations)
12. [Debugging & VM Mode](#debugging--vm-mode)
13. [Pitfalls & Gotchas](#pitfalls--gotchas)

---

## Introduction

C.ASM (C-Assembly) is a simplified ARM64 assembly dialect designed for EmberOS. Programs compile to real ARM64 machine code and execute natively on the CPU at full speed. Extended opcodes for I/O, graphics, and file operations use lightweight SVC traps handled by the kernel, providing both performance and safety.

### Why C.ASM?

- **Native speed** - Real ARM64 code runs directly on CPU
- **Safe execution** - Extended ops trap to kernel, programs can't crash the OS
- **Built-in I/O** - Simple opcodes: `prtc`, `prtn`, `inp` (no syscall setup)
- **Instant graphics** - Virtual framebuffer with `plot`, `line`, `box`, `canvas`
- **File access** - Read/write files with `fread`, `fwrite`, `fcopy`
- **Learning friendly** - Simpler than raw ARM64, great for learning assembly

### How It Works

```
┌──────────────────────────────────────────────────────────────────────┐
│                        C.ASM Execution Model                         │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   ┌─────────────┐      ┌─────────────┐      ┌─────────────┐         │
│   │ Source File │ ──▶  │    CASM     │ ──▶  │   Binary    │         │
│   │  (.asm)     │      │  Assembler  │      │   (.bin)    │         │
│   └─────────────┘      └─────────────┘      └─────────────┘         │
│                                                    │                 │
│                              ┌─────────────────────┘                 │
│                              ▼                                       │
│   ┌──────────────────────────────────────────────────────────────┐  │
│   │                    Native Execution                          │  │
│   │  ┌────────────────────────────────────────────────────────┐  │  │
│   │  │  ARM64 Instructions (mov, add, cmp, b, ldr, str...)   │  │  │
│   │  │  Execute directly on CPU at full speed                 │  │  │
│   │  └────────────────────────────────────────────────────────┘  │  │
│   │                           │                                  │  │
│   │                    SVC Trap (Extended Opcodes)               │  │
│   │                           ▼                                  │  │
│   │  ┌────────────────────────────────────────────────────────┐  │  │
│   │  │  Kernel SVC Handler                                    │  │  │
│   │  │  • I/O: prt, prtc, prtn, inp, inps                    │  │  │
│   │  │  • Graphics: plot, line, box, canvas, setc            │  │  │
│   │  │  • Files: fread, fwrite, fcreat, fdel                 │  │  │
│   │  │  • System: sleep, rnd, tick, halt                     │  │  │
│   │  └────────────────────────────────────────────────────────┘  │  │
│   └──────────────────────────────────────────────────────────────┘  │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### What C.ASM is NOT

- Not a high-level language (no variables, functions, or types)
- Not full ARM64 (subset of instructions supported)
- Not unlimited memory (5KB total for code + data)


---

## Architecture Overview

### Compilation Pipeline

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Two-Pass Assembly                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Source Code                                                        │
│       │                                                             │
│       ▼                                                             │
│  ┌─────────┐    Tokens    ┌─────────┐     AST     ┌─────────────┐  │
│  │  Lexer  │ ──────────▶  │ Parser  │ ──────────▶ │  Code Gen   │  │
│  └─────────┘              └─────────┘             └─────────────┘  │
│                                                          │         │
│                                                          ▼         │
│                                              ┌───────────────────┐ │
│                                              │  Pass 1: Labels   │ │
│                                              │  Collect symbols, │ │
│                                              │  calculate addrs  │ │
│                                              └─────────┬─────────┘ │
│                                                        │           │
│                                                        ▼           │
│                                              ┌───────────────────┐ │
│                                              │  Pass 2: Encode   │ │
│                                              │  Generate ARM64   │ │
│                                              │  machine code     │ │
│                                              └─────────┬─────────┘ │
│                                                        │           │
│                                                        ▼           │
│                                              ┌───────────────────┐ │
│                                              │  Binary Output    │ │
│                                              │  (.bin file)      │ │
│                                              └───────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

### Native Execution Model

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Runtime Memory Layout                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  code_buffer (5KB total)                                            │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ 0x0000 ┌──────────────────────────────────────────────────┐ │   │
│  │        │              Program Code                        │ │   │
│  │        │         (ARM64 machine code)                     │ │   │
│  │        │                                                  │ │   │
│  │ 0x0400 ├──────────────────────────────────────────────────┤ │   │
│  │        │              Data Area                           │ │   │
│  │        │    (strings, buffers, variables)                 │ │   │
│  │        │                                                  │ │   │
│  │ 0x1400 └──────────────────────────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  Reserved Registers (set by kernel before execution):               │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  x28 = code_buffer base (for data address translation)      │   │
│  │  x27 = framebuffer base pointer                             │   │
│  │  x26 = framebuffer row stride (81)                          │   │
│  │  x25 = color buffer base pointer                            │   │
│  │  x24 = current color value (set by setc/canvas/reset)       │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Extended Opcode Handling

Extended opcodes (I/O, graphics, files, system) are encoded as SVC instructions with specific immediate values. When the CPU executes an SVC, it traps to the kernel's exception handler which:

1. Reads the SVC number from the instruction
2. Reads/writes registers x0-x3 for parameters and return values
3. Performs the operation (UART I/O, file access, etc.)
4. Returns to the next instruction

This is extremely fast - just a few microseconds per SVC call.

---

## The CASM Compiler

### Quick Start

```bash
# Compile and run in one step (most common)
casm -r hello.asm

# Or compile first, then run
casm hello.asm -o hello.bin
casm run hello.bin
```

### All Commands

```bash
# Compile source to binary
casm source.asm -o output.bin

# Compile and run immediately (native execution)
casm -r source.asm

# Run compiled binary (native execution - fast!)
casm run program.bin

# Run in VM mode (for debugging or compatibility)
casm run -v program.bin

# Debug mode (step through instructions)
casm run -d program.bin

# Disassemble binary back to assembly
casm disasm program.bin
```

### Execution Modes Comparison

```
┌────────────────┬─────────────────────────────────────────────────────┐
│ Mode           │ Description                                         │
├────────────────┼─────────────────────────────────────────────────────┤
│ Native         │ Default. ARM64 code runs directly on CPU.           │
│ (default)      │ Extended opcodes trap via SVC. Very fast!           │
│                │ Use: casm run program.bin                           │
├────────────────┼─────────────────────────────────────────────────────┤
│ VM Mode        │ Interprets each instruction. Slower but useful      │
│ (-v flag)      │ for debugging or if native has issues.              │
│                │ Use: casm run -v program.bin                        │
├────────────────┼─────────────────────────────────────────────────────┤
│ Debug Mode     │ Step through instructions one at a time.            │
│ (-d flag)      │ Shows registers and flags after each step.          │
│                │ Use: casm run -d program.bin                        │
└────────────────┴─────────────────────────────────────────────────────┘
```

### Reserved Registers

Native execution reserves registers x24-x28. **Do not use these in your programs!**

| Register | Purpose |
|----------|---------|
| `x28` | Data base address (code_buffer + 0x400) |
| `x27` | Framebuffer base pointer |
| `x26` | Framebuffer row stride (81) |
| `x25` | Color buffer base pointer |
| `x24` | Current color value |

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
- `Parse error at line N` - Syntax error in source
- `Undefined symbol` - Label not found (check spelling)
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

## Tutorial: Your First Programs

### Hello World

The simplest program - print "Hi!" and exit:

```asm
; hello.asm - Your first C.ASM program
.text
_start:
    mov w0, #72      ; 'H' (ASCII 72)
    prtc             ; print character
    mov w0, #105     ; 'i' (ASCII 105)
    prtc
    mov w0, #33      ; '!' (ASCII 33)
    prtc
    mov w0, #10      ; newline
    prtc
    halt             ; stop execution
```

Run it:
```bash
casm -r hello.asm
```

Output: `Hi!`

### Adding Numbers

Print the result of 10 + 25:

```asm
; add.asm - Basic arithmetic
.text
_start:
    mov x0, #10      ; first number
    mov x1, #25      ; second number
    add x0, x0, x1   ; x0 = x0 + x1 = 35
    prtn             ; print number
    mov w0, #10      ; newline
    prtc
    halt
```

Output: `35`

### Counting Loop

Print numbers 1 to 5:

```asm
; count.asm - Loop example
.text
_start:
    mov x10, #1      ; counter starts at 1

loop:
    mov x0, x10      ; copy counter to x0 for printing
    prtn             ; print the number
    mov w0, #32      ; space character
    prtc
    
    add x10, x10, #1 ; increment counter
    cmp x10, #6      ; compare with 6
    b.lt loop        ; if less than 6, continue loop
    
    mov w0, #10      ; newline
    prtc
    halt
```

Output: `1 2 3 4 5`

### User Input

Read a character and echo it back:

```asm
; echo.asm - Input example
.text
_start:
    ; Print prompt
    mov w0, #63      ; '?'
    prtc
    mov w0, #32      ; space
    prtc
    
    ; Read character
    inp              ; wait for keypress, result in x0
    mov x10, x0      ; save the character
    
    ; Echo it back
    mov w0, #10      ; newline first
    prtc
    mov w0, #89      ; 'Y'
    prtc
    mov w0, #111     ; 'o'
    prtc
    mov w0, #117     ; 'u'
    prtc
    mov w0, #32      ; space
    prtc
    mov w0, #116     ; 't'
    prtc
    mov w0, #121     ; 'y'
    prtc
    mov w0, #112     ; 'p'
    prtc
    mov w0, #101     ; 'e'
    prtc
    mov w0, #100     ; 'd'
    prtc
    mov w0, #58      ; ':'
    prtc
    mov w0, #32      ; space
    prtc
    mov w0, w10      ; the saved character
    prtc
    mov w0, #10      ; newline
    prtc
    halt
```

### Simple Graphics

Draw a box on screen:

```asm
; box.asm - Graphics example
.text
_start:
    ; Set up 30x10 canvas
    mov w0, #30      ; width
    mov w1, #10      ; height
    canvas
    
    ; Set colors: yellow on blue
    mov w0, #3       ; yellow foreground
    mov w1, #4       ; blue background
    setc
    
    ; Draw a box at (2,1) size 20x6
    mov x0, #2       ; x position
    mov x1, #1       ; y position
    mov x2, #20      ; width
    mov x3, #6       ; height
    box
    
    ; Plot a star in the middle
    mov x0, #11      ; x
    mov x1, #3       ; y
    mov w2, #42      ; '*' character
    plot
    
    halt             ; renders the framebuffer
```

### Conditional Logic

Compare two numbers and print which is larger:

```asm
; compare.asm - Conditional branches
.text
_start:
    mov x10, #42     ; first number
    mov x11, #37     ; second number
    
    cmp x10, x11     ; compare x10 with x11
    b.gt first_bigger
    b.lt second_bigger
    
    ; They're equal
    mov w0, #61      ; '='
    prtc
    b done

first_bigger:
    mov w0, #62      ; '>'
    prtc
    b done

second_bigger:
    mov w0, #60      ; '<'
    prtc

done:
    mov w0, #10      ; newline
    prtc
    halt
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

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Register Usage Guide                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  General Purpose (safe to use):                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  x0-x7   │ Arguments and return values for extended ops     │   │
│  │  x8-x15  │ Temporary registers (use freely)                 │   │
│  │  x16-x23 │ More temporaries (use freely)                    │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  RESERVED - DO NOT USE:                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  x24     │ Current color (set by setc/canvas/reset)         │   │
│  │  x25     │ Color buffer base pointer                        │   │
│  │  x26     │ Framebuffer row stride (81)                      │   │
│  │  x27     │ Framebuffer base pointer                         │   │
│  │  x28     │ Data base address (code_buffer)                  │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  Special:                                                           │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  x30/lr  │ Link register (return address for bl/ret)        │   │
│  │  x31/sp  │ Stack pointer / Zero register (context-dependent)│   │
│  │  xzr/wzr │ Zero register (always reads as 0)                │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

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

Extended opcodes are C.ASM-specific instructions that provide high-level functionality. They're encoded as `SVC` instructions with special immediate values, handled by the kernel.

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Extended Opcode Categories                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  I/O (0x100-0x10F)          Graphics (0x110-0x11F)                 │
│  ┌─────────────────┐        ┌─────────────────┐                    │
│  │ prt   - string  │        │ canvas - size   │                    │
│  │ prtc  - char    │        │ cls    - clear  │                    │
│  │ prtn  - number  │        │ setc   - colors │                    │
│  │ prtx  - hex     │        │ plot   - pixel  │                    │
│  │ inp   - char in │        │ line   - line   │                    │
│  │ inps  - string  │        │ box    - rect   │                    │
│  └─────────────────┘        │ reset  - reset  │                    │
│                             └─────────────────┘                    │
│  Files (0x120-0x12F)        System (0x1F0-0x1FF)                   │
│  ┌─────────────────┐        ┌─────────────────┐                    │
│  │ fcreat - create │        │ sleep  - delay  │                    │
│  │ fwrite - write  │        │ rnd    - random │                    │
│  │ fread  - read   │        │ tick   - time   │                    │
│  │ fdel   - delete │        │ halt   - stop   │                    │
│  │ fcopy  - copy   │        └─────────────────┘                    │
│  │ fmove  - rename │                                               │
│  │ fexist - exists │        Memory (0x130-0x13F)                   │
│  └─────────────────┘        ┌─────────────────┐                    │
│                             │ strlen - length │                    │
│                             │ memcpy - copy   │                    │
│                             │ memset - fill   │                    │
│                             │ abs    - abs    │                    │
│                             └─────────────────┘                    │
└─────────────────────────────────────────────────────────────────────┘
```

### I/O Operations

#### prt - Print String
Prints null-terminated string at address in x0.
```asm
mov x0, #0x500       ; Address of string
prt                  ; Print it
```

#### prtc - Print Character
Prints single character from w0. Output is buffered for performance.
```asm
mov w0, #65          ; 'A'
prtc                 ; Prints: A
```

#### prtn - Print Number
Prints x0 as signed decimal number.
```asm
mov x0, #42
prtn                 ; Prints: 42
```

#### prtx - Print Hex
Prints x0 as hexadecimal with 0x prefix.
```asm
mov x0, #255
prtx                 ; Prints: 0xff
```

#### inp - Input Character
Waits for keypress, stores ASCII in x0. Flushes output buffer first.
```asm
inp                  ; Wait for key
; x0 now contains the character code
```

#### inps - Input String
Reads line of text into buffer. Supports backspace editing.
```asm
mov x0, #0x500       ; Buffer address
mov x1, #64          ; Max length
inps                 ; Read input
; x0 = actual length read (not including null)
```

### Graphics Operations

Graphics use a virtual framebuffer that renders when the program ends.

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Graphics Coordinate System                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  (0,0)────────────────────────────────────▶ X (width, max 80)      │
│    │                                                                │
│    │   ┌─────────────────────────────┐                             │
│    │   │  Your canvas area           │                             │
│    │   │                             │                             │
│    │   │     * (5,2) = plot here     │                             │
│    │   │                             │                             │
│    │   └─────────────────────────────┘                             │
│    ▼                                                                │
│    Y (height, max 24)                                               │
│                                                                     │
│  Colors: 0=black 1=red 2=green 3=yellow 4=blue 5=magenta 6=cyan 7=white │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

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
┌─────────────────────────────────────────────────────────────────────┐
│                    C.ASM Memory Map (5KB Total)                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  0x0000 ┌───────────────────────────────────────────────────────┐  │
│         │                                                       │  │
│         │              PROGRAM CODE                             │  │
│         │         (your compiled instructions)                  │  │
│         │                                                       │  │
│         │  Typical size: 100-500 bytes for small programs       │  │
│         │                                                       │  │
│  0x0400 ├───────────────────────────────────────────────────────┤  │
│         │                                                       │  │
│         │              DATA AREA                                │  │
│         │                                                       │  │
│  0x0500 │  ┌─────────────────────────────────────────────────┐  │  │
│         │  │  Recommended data start (0x500)                 │  │  │
│         │  │                                                 │  │  │
│         │  │  • String buffers                               │  │  │
│         │  │  • Input buffers                                │  │  │
│         │  │  • File content                                 │  │  │
│         │  │  • Variables                                    │  │  │
│         │  │                                                 │  │  │
│  0x1400 │  └─────────────────────────────────────────────────┘  │  │
│         └───────────────────────────────────────────────────────┘  │
│                                                                     │
│  Note: Addresses 0x400-0x1FFF are automatically translated to      │
│  x28-relative addressing for native execution compatibility.        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Memory Regions

| Address Range | Size | Recommended Usage |
|---------------|------|-------------------|
| 0x0000 - 0x03FF | 1KB | Program code |
| 0x0400 - 0x04FF | 256B | Small variables, counters |
| 0x0500 - 0x05FF | 256B | Filename buffers |
| 0x0600 - 0x07FF | 512B | Input/output buffers |
| 0x0800 - 0x0FFF | 2KB | File content, large data |
| 0x1000 - 0x13FF | 1KB | Additional space |

### Best Practices

1. **Start data at 0x500** - Leaves room for code growth
2. **Use fixed addresses** - No dynamic allocation available
3. **Plan buffer sizes** - Know your limits before coding
4. **Null-terminate strings** - Required for `prt`, `strlen`
5. **Align large buffers** - 8-byte alignment helps `memcpy`/`memset`

### Example Memory Usage

```asm
; Memory map for a file processing program:
;
; 0x500-0x53F: filename buffer (64 bytes)
; 0x540-0x5FF: input buffer (192 bytes)  
; 0x600-0x7FF: file content buffer (512 bytes)
; 0x800-0x8FF: output buffer (256 bytes)

.text
_start:
    mov x10, #0x500      ; filename buffer pointer
    mov x11, #0x540      ; input buffer pointer
    mov x12, #0x600      ; file buffer pointer
    mov x13, #0x800      ; output buffer pointer
    
    ; Now use these pointers throughout your program
    ; Example: read filename into x10 buffer
    mov x0, x10
    mov x1, #63          ; max 63 chars + null
    inps
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

### Printing Multiple Characters```asm
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

## Performance Optimizations

Native execution includes several optimizations for maximum performance:

### Batched UART Output

Character output is buffered (256 bytes) and flushed automatically:
- On newline character (`\n`)
- Before input operations (`inp`, `inps`)
- Before `sleep` (so text appears before delay)
- On program exit (`halt`)

This dramatically reduces MMIO overhead for programs that print many characters.

### Optimized Memory Operations

`memcpy` and `memset` use 64-bit word operations when addresses are 8-byte aligned:

```
┌─────────────────────────────────────────────────────────────────────┐
│  memcpy/memset Optimization                                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Unaligned (byte-by-byte):     Aligned (word-by-word):             │
│  ┌───┬───┬───┬───┬───┬───┐    ┌───────────────────────┐            │
│  │ B │ B │ B │ B │ B │ B │    │    8 bytes at once    │            │
│  └───┴───┴───┴───┴───┴───┘    └───────────────────────┘            │
│  6 operations                  1 operation (8x faster!)             │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Direct Framebuffer Access

The `plot` opcode writes directly to the framebuffer without SVC overhead:
- Kernel sets up x27 (framebuffer), x26 (stride), x25 (colors), x24 (color)
- `plot` generates inline MADD + STRB instructions
- No trap to kernel needed for each pixel!

### Peephole Optimization

The code generator eliminates no-op instructions:

| Pattern | Result |
|---------|--------|
| `mov xN, xN` | Eliminated (no-op) |
| `add xN, xN, #0` | Eliminated (no-op) |
| `sub xN, xN, #0` | Eliminated (no-op) |

Works for both 32-bit (wN) and 64-bit (xN) registers.

### Fast RNG

The `rnd` opcode uses xorshift64 algorithm - faster and better quality than simple LCG.

---

## Debugging & VM Mode

### When to Use VM Mode

VM mode (`-v` flag) interprets each instruction instead of running native code. Use it when:

- Debugging complex logic issues
- Native execution behaves unexpectedly
- You need to trace exact instruction flow
- Testing on systems without ARM64 hardware

```bash
# Run in VM mode
casm run -v program.bin
```

### Debug Mode

Debug mode (`-d` flag) lets you step through instructions one at a time:

```bash
casm run -d program.bin
```

Output shows:
```
Debug mode: Press ENTER to step, 'r' to run, 'q' to quit

PC=0000: 52800900  mov w0, #72
  x0-x3: 00000000 00000000 00000000 00000000
  Flags: N=0 Z=0 C=0 V=0
```

Commands:
- `ENTER` - Execute one instruction
- `r` - Run to completion
- `q` - Quit

### Disassembly

View compiled code to understand what was generated:

```bash
casm disasm program.bin
```

Output:
```
Disassembly of 'program.bin' (24 bytes):

0000:  52800900  mov w0, #72
0004:  d4002021  prtc
0008:  52800540  mov w0, #42
000c:  d4002021  prtc
0010:  d4003fe1  halt
```

### Debugging Tips

1. **Add print statements** - Use `prtn` to show register values
2. **Check addresses** - Print pointers with `prtx` to verify
3. **Verify strings** - Use `strlen` to check string length
4. **Step through** - Use debug mode for complex logic
5. **Check labels** - Disassemble to verify branch targets

### Debug Pattern Example

```asm
    ; Debug: print "D:" then register value
    mov x8, x0           ; save x0
    mov w0, #68          ; 'D'
    prtc
    mov w0, #58          ; ':'
    prtc
    mov x0, x8           ; restore for print
    prtn                 ; print the value
    mov w0, #10          ; newline
    prtc
    mov x0, x8           ; restore x0 for continued use
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

### 11. Using Reserved Registers (x24-x28)

**Critical for native execution!** Registers x24-x28 are reserved by the kernel:

```asm
; WRONG - will break native execution!
mov x28, #100        ; x28 is reserved for data base!
mov x27, #0          ; x27 is reserved for framebuffer!
add x25, x25, #1     ; x25 is reserved for colors!

; RIGHT - use x0-x23 instead
mov x20, #100
mov x21, #0
add x22, x22, #1
```

If your program works in VM mode (`-v`) but crashes in native mode, check for reserved register usage.

---

## ASCII Quick Reference

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Common ASCII Values                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Control:           Punctuation:         Digits:                    │
│  ┌────────────┐     ┌────────────┐       ┌────────────┐            │
│  │ newline=10 │     │ space=32   │       │ '0' = 48   │            │
│  │ tab=9      │     │ !=33 "=34  │       │ '1' = 49   │            │
│  │ return=13  │     │ #=35 $=36  │       │ ...        │            │
│  └────────────┘     │ %=37 &=38  │       │ '9' = 57   │            │
│                     │ '=39 (=40  │       └────────────┘            │
│  Uppercase:         │ )=41 *=42  │                                 │
│  ┌────────────┐     │ +=43 ,=44  │       Lowercase:                │
│  │ 'A' = 65   │     │ -=45 .=46  │       ┌────────────┐            │
│  │ 'B' = 66   │     │ /=47 :=58  │       │ 'a' = 97   │            │
│  │ ...        │     │ ;=59 <=60  │       │ 'b' = 98   │            │
│  │ 'Z' = 90   │     │ ==61 >=62  │       │ ...        │            │
│  └────────────┘     │ ?=63 @=64  │       │ 'z' = 122  │            │
│                     └────────────┘       └────────────┘            │
│                                                                     │
│  Quick formula: lowercase = uppercase + 32                          │
│                 digit_value = char - 48                             │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

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
