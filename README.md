# EmberOS

A lightweight ARM64 operating system for QEMU virt platform.

## Quick Start

```bash
make clean && make
make run
```

## Shell Commands

### Navigation & Files
```
ls [path]        - List directory contents
cd <path>        - Change directory
pwd              - Print working directory
cat <file>       - Display file contents
cp <src> <dst>   - Copy file
mv <src> <dst>   - Move/rename file
rm [-rf] <path>  - Remove file or directory
mkdir <dir>      - Create directory
rmdir <dir>      - Remove empty directory
touch <file>     - Create empty file
find <path> [pattern] - Search for files
```

### File Editing
```
vi <file>        - Text editor (vim-like)
write <file>     - Simple text input (Ctrl+D to save)
xxd <file>       - Hex dump of file
```

### System Info
```
help [category]  - Show help (system/files/dev/shell)
uptime           - System uptime
meminfo          - Memory statistics
cpuinfo          - CPU information
df               - Filesystem usage
ps               - List processes
top              - Interactive process viewer
regs             - Display CPU registers
```

### System Control
```
clear            - Clear screen
reboot           - Restart system
shutdown         - Halt system
```

### Developer Tools
```
casm <file.asm>           - Assemble C.ASM source
casm -r <file.asm>        - Compile and run directly
casm run <file.bin>       - Run compiled binary
casm run -d <file.bin>    - Debug mode (step through)
casm disasm <file.bin>    - Disassemble binary
hexdump <addr> [len]      - Dump memory
```

---

# C.ASM - EmberOS Assembly Language

C.ASM is a simplified ARM64 assembly dialect with built-in I/O, graphics, and system opcodes. It's designed for writing small programs directly on EmberOS.

## How C.ASM Differs from Standard ARM64 ASM

| Feature | Standard ARM64 | C.ASM |
|---------|---------------|-------|
| I/O | System calls, complex setup | Simple opcodes: `prtc`, `prtn`, `inp` |
| Graphics | Framebuffer drivers | Built-in: `plot`, `line`, `box` |
| Files | Kernel syscalls | Direct: `fwrite`, `fread`, `fcopy` |
| Strings | Manual loops | `strlen`, `memcpy`, `memset` |
| Execution | Native CPU | Interpreted VM (safe sandbox) |

## Basic Syntax

```asm
; Comments start with semicolon
.text
_start:
    mov x0, #65      ; Load 'A' into x0
    prtc             ; Print character
    halt             ; Stop execution
```

## Registers

- `x0-x30` - 64-bit general purpose registers
- `w0-w30` - 32-bit (lower half of x registers)
- `sp` - Stack pointer (x31)
- `lr` - Link register (x30)

## Supported ARM64 Instructions

### Data Movement
```asm
mov x0, #123         ; Load immediate
mov x0, x1           ; Register to register
movz x0, #0x1234     ; Move with zero
movn x0, #0          ; Move NOT (for negative numbers)
```

### Arithmetic
```asm
add x0, x1, #10      ; x0 = x1 + 10
add x0, x1, x2       ; x0 = x1 + x2
sub x0, x1, #5       ; x0 = x1 - 5
mul x0, x1, x2       ; x0 = x1 * x2
```

### Compare & Branch
```asm
cmp x0, #10          ; Compare x0 with 10
cmp x0, x1           ; Compare registers
b.eq label           ; Branch if equal
b.ne label           ; Branch if not equal
b.lt label           ; Branch if less than
b.gt label           ; Branch if greater than
b.ge label           ; Branch if greater or equal
b.le label           ; Branch if less or equal
b label              ; Unconditional branch
bl label             ; Branch with link (call)
ret                  ; Return
```

### Memory Access
```asm
strb w0, [x1]        ; Store byte
strb w0, [x1, #5]    ; Store byte at offset
ldrb w0, [x1]        ; Load byte
str x0, [x1]         ; Store 64-bit
ldr x0, [x1]         ; Load 64-bit
```

---

## C.ASM Extended Opcodes

### I/O Operations
| Opcode | Description | Usage |
|--------|-------------|-------|
| `prt` | Print string at address | `mov x0, #addr` then `prt` |
| `prtc` | Print character | `mov w0, #65` then `prtc` → 'A' |
| `prtn` | Print number | `mov x0, #42` then `prtn` → "42" |
| `prtx` | Print hex | `mov x0, #255` then `prtx` → "0xff" |
| `inp` | Input single char | `inp` → char in x0 |
| `inps` | Input string | `mov x0, #buf` `mov x1, #maxlen` `inps` |

### Graphics (Auto-creates 40x12 canvas)
| Opcode | Description | Registers |
|--------|-------------|-----------|
| `canvas` | Set canvas size | x0=width, x1=height |
| `cls` | Clear screen | - |
| `setc` | Set colors | x0=fg (0-7), x1=bg (0-7) |
| `plot` | Draw character | x0=x, x1=y, x2=char |
| `line` | Draw line | x0=x1, x1=y1, x2=x2, x3=y2, x4=char |
| `box` | Draw box | x0=x, x1=y, x2=width, x3=height |
| `reset` | Reset colors | - |

Colors: 0=black, 1=red, 2=green, 3=yellow, 4=blue, 5=magenta, 6=cyan, 7=white

### File Operations
| Opcode | Description | Registers |
|--------|-------------|-----------|
| `fcreat` | Create file | x0=name_ptr → x0=success |
| `fwrite` | Write file | x0=name_ptr, x1=data_ptr, x2=len |
| `fread` | Read file | x0=name_ptr, x1=buf_ptr, x2=maxlen → x0=bytes |
| `fdel` | Delete file | x0=name_ptr |
| `fcopy` | Copy file | x0=src_ptr, x1=dst_ptr |
| `fmove` | Move/rename | x0=src_ptr, x1=dst_ptr |
| `fexist` | Check exists | x0=name_ptr → x0=1 if exists |

### Memory/String Operations
| Opcode | Description | Registers |
|--------|-------------|-----------|
| `strlen` | String length | x0=addr → x0=length |
| `memcpy` | Copy memory | x0=dst, x1=src, x2=len |
| `memset` | Fill memory | x0=addr, x1=byte, x2=len |
| `abs` | Absolute value | x0=val → x0=\|val\| |

### System
| Opcode | Description | Registers |
|--------|-------------|-----------|
| `sleep` | Delay ms | x0=milliseconds |
| `rnd` | Random number | x0=max → x0=random(0..max-1) |
| `tick` | Get uptime ms | → x0=milliseconds |
| `halt` | Stop program | - |

---

## Example Programs

### Hello World
```asm
; hello.asm - Print "Hi"
.text
_start:
    mov w0, #72      ; 'H'
    prtc
    mov w0, #105     ; 'i'
    prtc
    mov w0, #10      ; newline
    prtc
    halt
```

### Counter Loop
```asm
; Print 1 to 5
.text
_start:
    mov x10, #1      ; counter
loop:
    mov x0, x10
    prtn             ; print number
    mov w0, #32      ; space
    prtc
    add x10, x10, #1
    cmp x10, #6
    b.lt loop
    halt
```

### Draw Box
```asm
; Draw a box with graphics
.text
_start:
    mov x0, #5       ; x
    mov x1, #2       ; y
    mov x2, #20      ; width
    mov x3, #6       ; height
    box
    halt
```

### File I/O
```asm
; Write and read a file
.text
_start:
    ; Build filename "test.txt" at 0x500
    mov x10, #0x500
    mov w0, #116     ; 't'
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #101     ; 'e'
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #115     ; 's'
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #116     ; 't'
    strb w0, [x10]
    add x10, x10, #1
    mov w0, #0       ; null
    strb w0, [x10]
    
    ; Build content "Hello" at 0x520
    mov x10, #0x520
    mov w0, #72      ; 'H'
    strb w0, [x10]
    ; ... (continue for rest of string)
    
    ; Write file
    mov x0, #0x500   ; filename
    mov x1, #0x520   ; data
    mov x2, #5       ; length
    fwrite
    
    halt
```

---

## Running C.ASM Programs

### Method 1: Compile and Run Separately
```
ember:/> casm hello.asm -o hello.bin
ember:/> casm run hello.bin
```

### Method 2: Compile and Run Directly
```
ember:/> casm -r hello.asm
```

### Debug Mode (Step Through)
```
ember:/> casm run -d hello.bin
```

### Disassemble Binary
```
ember:/> casm disasm hello.bin
```

---

## Example Files

Check the `examples/` directory for demo programs:

| File | Description |
|------|-------------|
| `hello.asm` | Hello world |
| `simple.asm` | Basic arithmetic |
| `loop.asm` | Nested loops, printing patterns |
| `box.asm` | Graphics demo |
| `graphics.asm` | Colors and shapes |
| `countdown.asm` | Timer with tick opcode |
| `guessing.asm` | Number guessing game |
| `fileio.asm` | File operations demo |
| `memtest.asm` | Memory/string ops test |

---

## Memory Layout

C.ASM programs have 5KB of memory:
- Code starts at address 0x0
- Use addresses 0x500+ for data buffers
- Strings must be null-terminated

## Tips

1. Use `w` registers for characters (8-bit values)
2. Use `x` registers for addresses and large numbers
3. Always null-terminate strings for `prt` and `strlen`
4. Graphics auto-initialize on first use (40x12 default)
5. Check `examples/` for working code patterns

## ASCII Reference

Common values: A=65, a=97, 0=48, space=32, newline=10
