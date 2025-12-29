# EmberOS System Diagrams

Visual diagrams showing how EmberOS components interact.

---

## 1. System Boot Sequence

```
+------------------------------------------------------------------+
|                       Boot Sequence                              |
+------------------------------------------------------------------+
|                                                                  |
|  QEMU Loads kernel.elf                                           |
|         |                                                        |
|         v                                                        |
|  +-------------+                                                 |
|  | _start      |  (boot.S)                                       |
|  | - Set SP    |                                                 |
|  | - Clear BSS |                                                 |
|  +-------------+                                                 |
|         |                                                        |
|         v                                                        |
|  +-------------+                                                 |
|  | kernel_main |  (main.cpp)                                     |
|  +-------------+                                                 |
|         |                                                        |
|         +---> memory::init()      Initialize page allocator      |
|         |                                                        |
|         +---> uart::init()        Initialize UART driver         |
|         |                                                        |
|         +---> interrupts::init()  Install exception vectors      |
|         |                                                        |
|         +---> gic::init()         Initialize GIC                 |
|         |                                                        |
|         +---> timer::init()       Initialize system timer        |
|         |                                                        |
|         +---> ramfs::init()       Initialize filesystem          |
|         |                                                        |
|         +---> process::init()     Initialize process table       |
|         |                                                        |
|         +---> uart::enable_rx_irq() Enable UART interrupts       |
|         |                                                        |
|         +---> interrupts::enable() Enable global interrupts      |
|         |                                                        |
|         v                                                        |
|  +-------------+                                                 |
|  | shell::run  |  Start interactive shell                        |
|  +-------------+                                                 |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 2. Interrupt Flow

```
+------------------------------------------------------------------+
|                      Interrupt Flow                              |
+------------------------------------------------------------------+
|                                                                  |
|  Hardware Event (UART RX / Timer)                                |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  |   GIC (IRQ 33)   |  Generic Interrupt Controller              |
|  +------------------+                                            |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  |   CPU IRQ Line   |  Signals pending interrupt                 |
|  +------------------+                                            |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Exception Vector |  vectors.S: irq_handler                    |
|  +------------------+                                            |
|         |                                                        |
|         +---> Save context (x0-x30, SP, ELR, SPSR)               |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | handle_irq()     |  interrupts.cpp                            |
|  +------------------+                                            |
|         |                                                        |
|         +---> irq = gic::acknowledge_irq()                       |
|         |                                                        |
|         +---> irq_handlers[irq]()   Call registered handler      |
|         |         |                                              |
|         |         +---> uart_irq_handler() for IRQ 33            |
|         |         |         |                                    |
|         |         |         +---> Read char from UART            |
|         |         |         +---> Store in ring buffer           |
|         |         |         +---> Update statistics              |
|         |         |                                              |
|         |         +---> timer_irq_handler() for IRQ 30           |
|         |                   |                                    |
|         |                   +---> Update uptime                  |
|         |                   +---> scheduler_tick()               |
|         |                                                        |
|         +---> gic::end_irq(irq)                                  |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Restore context  |  ERET back to interrupted code             |
|  +------------------+                                            |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 3. Shell Command Execution

```
+------------------------------------------------------------------+
|                   Shell Command Execution                        |
+------------------------------------------------------------------+
|                                                                  |
|  User types command                                              |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | shell::run()     |  Main shell loop                           |
|  +------------------+                                            |
|         |                                                        |
|         +---> print_prompt()  "ember:/> "                        |
|         |                                                        |
|         +---> read_line()     Wait for input                     |
|         |         |                                              |
|         |         +---> while (no input) {                       |
|         |         |         run_background_slice()               |
|         |         |         check_background_jobs()              |
|         |         |         WFI  // Wait for interrupt           |
|         |         |     }                                        |
|         |         +---> getc_nonblocking()                       |
|         |                                                        |
|         +---> parse_command()                                    |
|         |         |                                              |
|         |         +---> Split into argc/argv                     |
|         |         +---> Check for "nohup" prefix                 |
|         |         +---> Check for aliases                        |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | execute_command  |                                            |
|  +------------------+                                            |
|         |                                                        |
|         +---> Is it "nohup"?                                     |
|         |         |                                              |
|         |    Yes  +---> create_background()                      |
|         |         |         |                                    |
|         |         |         +---> Create PCB                     |
|         |         |         +---> Setup bg_task                  |
|         |         |         +---> Print "[N] appending..."       |
|         |         |         +---> Return to shell                |
|         |         |                                              |
|         |    No   +---> Foreground execution                     |
|         |               |                                        |
|         |               +---> Create process entry               |
|         |               +---> cmd_handler(argc, argv)            |
|         |               +---> Reap process                       |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Return to prompt |                                            |
|  +------------------+                                            |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 4. CASM Program Execution

```
+------------------------------------------------------------------+
|                   CASM Program Execution                         |
+------------------------------------------------------------------+
|                                                                  |
|  User runs: casm -r hello.asm                                    |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | cmd_casm()       |  commands.cpp                              |
|  +------------------+                                            |
|         |                                                        |
|         +---> Read source file from RAMFS                        |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Lexer            |  casm/lexer.cpp                            |
|  +------------------+                                            |
|         |                                                        |
|         +---> Tokenize source                                    |
|         |     (labels, instructions, operands)                   |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Parser           |  casm/parser.cpp                           |
|  +------------------+                                            |
|         |                                                        |
|         +---> Build AST                                          |
|         |     (instruction nodes with operands)                  |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Code Generator   |  casm/codegen.cpp                          |
|  +------------------+                                            |
|         |                                                        |
|         +---> Pass 1: Collect labels, calculate addresses        |
|         |                                                        |
|         +---> Pass 2: Encode ARM64 instructions                  |
|         |         |                                              |
|         |         +---> Standard ARM64 -> native encoding        |
|         |         +---> Extended opcodes -> SVC #N               |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Binary Output    |  5KB code buffer                           |
|  +------------------+                                            |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | casm_native::run |  Native execution                          |
|  +------------------+                                            |
|         |                                                        |
|         +---> Setup reserved registers (x24-x28)                 |
|         |                                                        |
|         +---> BLR to code_buffer (jump to code)                  |
|         |         |                                              |
|         |         +---> ARM64 instructions execute directly      |
|         |         |                                              |
|         |         +---> SVC #N traps to kernel                   |
|         |                   |                                    |
|         |                   v                                    |
|         |            +------------------+                        |
|         |            | SVC Handler      |                        |
|         |            +------------------+                        |
|         |                   |                                    |
|         |                   +---> 0x101: prtc -> uart::putc()    |
|         |                   +---> 0x102: prtn -> print number    |
|         |                   +---> 0x112: plot -> framebuffer     |
|         |                   +---> 0x1FF: halt -> return          |
|         |                   |                                    |
|         |                   +---> ERET back to code              |
|         |                                                        |
|         +---> Cleanup (render framebuffer if used)               |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 5. Background Task Lifecycle

```
+------------------------------------------------------------------+
|                  Background Task Lifecycle                       |
+------------------------------------------------------------------+
|                                                                  |
|  User runs: nohup sleep 2000                                     |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | cmd_nohup()      |                                            |
|  +------------------+                                            |
|         |                                                        |
|         +---> create_background("sleep", "sleep 2000", ...)      |
|         |         |                                              |
|         |         +---> Allocate PCB slot                        |
|         |         +---> Set state = RUNNING                      |
|         |         +---> Set background = true                    |
|         |         +---> Setup bg_task structure                  |
|         |         +---> Calculate target_end time                |
|         |                                                        |
|         +---> Print "[2] appending output to nohup.out"          |
|         |                                                        |
|         +---> Return to shell (non-blocking)                     |
|                                                                  |
|  Shell continues...                                              |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | Waiting for      |  (in getc syscall)                         |
|  | user input       |                                            |
|  +------------------+                                            |
|         |                                                        |
|         +---> run_background_slice()                             |
|         |         |                                              |
|         |         +---> Check if now >= target_end               |
|         |         |         |                                    |
|         |         |    No   +---> Return (still sleeping)        |
|         |         |         |                                    |
|         |         |    Yes  +---> Mark task complete             |
|         |         |               |                              |
|         |         |               +---> proc->state = ZOMBIE     |
|         |         |               +---> bg_task.active = false   |
|         |         |               +---> Flush nohup.out          |
|         |                                                        |
|         +---> check_background_jobs()                            |
|         |         |                                              |
|         |         +---> Find ZOMBIE with background=true         |
|         |         +---> Print "[2] Done: sleep 2000"             |
|         |         +---> Set notified = true                      |
|         |                                                        |
|         +---> reap_zombies()                                     |
|                   |                                              |
|                   +---> Free PCB slot                            |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 6. Memory Layout

```
+------------------------------------------------------------------+
|                      Memory Layout                               |
+------------------------------------------------------------------+
|                                                                  |
|  0x00000000 +--------------------------------------------------+ |
|             |                                                  | |
|             |              Reserved                            | |
|             |                                                  | |
|  0x40000000 +--------------------------------------------------+ |
|             |                                                  | |
|             |              Kernel Code                         | |
|             |              (.text section)                     | |
|             |                                                  | |
|  0x40080000 +--------------------------------------------------+ |
|             |                                                  | |
|             |              Kernel Data                         | |
|             |              (.data, .bss)                       | |
|             |                                                  | |
|  0x40100000 +--------------------------------------------------+ |
|             |                                                  | |
|             |              Kernel Stack                        | |
|             |              (grows down)                        | |
|             |                                                  | |
|  0x41000000 +--------------------------------------------------+ |
|             |                                                  | |
|             |              Page Allocator                      | |
|             |              (free pages)                        | |
|             |                                                  | |
|  0x48000000 +--------------------------------------------------+ |
|             |                                                  | |
|             |              End of RAM (128MB)                  | |
|             |                                                  | |
+------------------------------------------------------------------+
|                                                                  |
|  CASM Program Memory (5KB buffer):                               |
|                                                                  |
|  0x0000 +--------------------------------------------------+     |
|         |              Program Code                        |     |
|         |              (ARM64 machine code)                |     |
|  0x0400 +--------------------------------------------------+     |
|         |              Data Area                           |     |
|         |              (strings, buffers)                  |     |
|  0x1400 +--------------------------------------------------+     |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 7. Exception Vector Table

```
+------------------------------------------------------------------+
|                   Exception Vector Table                         |
+------------------------------------------------------------------+
|                                                                  |
|  VBAR_EL1 points to vector table (aligned to 2KB)                |
|                                                                  |
|  Offset    Exception Type              Handler                   |
|  ------    --------------              -------                   |
|                                                                  |
|  Current EL with SP_EL0:                                         |
|  0x000     Synchronous                 sync_handler              |
|  0x080     IRQ                         irq_handler               |
|  0x100     FIQ                         fiq_handler               |
|  0x180     SError                      serror_handler            |
|                                                                  |
|  Current EL with SP_ELx:                                         |
|  0x200     Synchronous                 sync_handler              |
|  0x280     IRQ                         irq_handler               |
|  0x300     FIQ                         fiq_handler               |
|  0x380     SError                      serror_handler            |
|                                                                  |
|  Lower EL using AArch64:                                         |
|  0x400     Synchronous                 sync_lower_handler        |
|  0x480     IRQ                         irq_lower_handler         |
|  0x500     FIQ                         fiq_handler               |
|  0x580     SError                      serror_handler            |
|                                                                  |
|  Lower EL using AArch32:                                         |
|  0x600     Synchronous                 (not used)                |
|  0x680     IRQ                         (not used)                |
|  0x700     FIQ                         (not used)                |
|  0x780     SError                      (not used)                |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 8. GIC (Generic Interrupt Controller)

```
+------------------------------------------------------------------+
|                   GIC Architecture                               |
+------------------------------------------------------------------+
|                                                                  |
|  +------------------+     +------------------+                   |
|  |   Distributor    |     |   CPU Interface  |                   |
|  |   (GICD)         |     |   (GICC)         |                   |
|  +------------------+     +------------------+                   |
|  | 0x08000000       |     | 0x08010000       |                   |
|  +------------------+     +------------------+                   |
|         |                        |                               |
|         |  IRQ Routing           |  IRQ Signaling                |
|         |                        |                               |
|         v                        v                               |
|  +--------------------------------------------------+            |
|  |                    CPU Core                      |            |
|  +--------------------------------------------------+            |
|                                                                  |
|  Key Registers:                                                  |
|                                                                  |
|  GICD (Distributor):                                             |
|  +------------------+----------------------------------+         |
|  | GICD_CTLR        | Enable distributor               |         |
|  | GICD_ISENABLER   | Enable specific IRQs             |         |
|  | GICD_ICENABLER   | Disable specific IRQs            |         |
|  | GICD_IPRIORITYR  | Set IRQ priorities               |         |
|  | GICD_ITARGETSR   | Set IRQ target CPUs              |         |
|  +------------------+----------------------------------+         |
|                                                                  |
|  GICC (CPU Interface):                                           |
|  +------------------+----------------------------------+         |
|  | GICC_CTLR        | Enable CPU interface             |         |
|  | GICC_PMR         | Priority mask                    |         |
|  | GICC_IAR         | Acknowledge IRQ (read)           |         |
|  | GICC_EOIR        | End of IRQ (write)               |         |
|  +------------------+----------------------------------+         |
|                                                                  |
|  IRQ Numbers:                                                    |
|  +------------------+----------------------------------+         |
|  | 0-15             | SGI (Software Generated)         |         |
|  | 16-31            | PPI (Private Peripheral)         |         |
|  |   30             |   Timer                          |         |
|  | 32-1019          | SPI (Shared Peripheral)          |         |
|  |   33             |   UART0                          |         |
|  +------------------+----------------------------------+         |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 9. UART Ring Buffer

```
+------------------------------------------------------------------+
|                    UART Ring Buffer                              |
+------------------------------------------------------------------+
|                                                                  |
|  256-byte circular buffer for received characters                |
|                                                                  |
|  +----------------------------------------------------------+    |
|  |  0   1   2   3   4   5   6   7   ...  253 254 255        |    |
|  +----------------------------------------------------------+    |
|     ^                       ^                                    |
|     |                       |                                    |
|   tail                    head                                   |
|  (read)                  (write)                                 |
|                                                                  |
|  Operations:                                                     |
|                                                                  |
|  IRQ Handler (write):                                            |
|  +----------------------------------------------------------+    |
|  |  char c = UART_DR;           // Read from hardware        |   |
|  |  buffer[head] = c;           // Store in buffer           |   |
|  |  head = (head + 1) % 256;    // Advance head              |   |
|  +----------------------------------------------------------+    |
|                                                                  |
|  getc_nonblocking (read):                                        |
|  +----------------------------------------------------------+    |
|  |  if (head == tail) return -1;  // Empty                   |   |
|  |  char c = buffer[tail];        // Read from buffer        |   |
|  |  tail = (tail + 1) % 256;      // Advance tail            |   |
|  |  return c;                                                |   |
|  +----------------------------------------------------------+    |
|                                                                  |
|  buffer_count:                                                   |
|  +----------------------------------------------------------+    |
|  |  return (head - tail + 256) % 256;                        |   |
|  +----------------------------------------------------------+    |
|                                                                  |
|  Example state:                                                  |
|                                                                  |
|  After receiving "hello":                                        |
|  +----------------------------------------------------------+    |
|  | 'h' 'e' 'l' 'l' 'o'  _   _   _   ...                     |    |
|  +----------------------------------------------------------+    |
|     ^                       ^                                    |
|   tail=0                  head=5                                 |
|                                                                  |
|  After reading "hel":                                            |
|  +----------------------------------------------------------+   |
|  | 'h' 'e' 'l' 'l' 'o'  _   _   _   ...                     |   |
|  +----------------------------------------------------------+   |
|               ^             ^                                    |
|             tail=3        head=5                                 |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 10. Process Table

```
+------------------------------------------------------------------+
|                      Process Table                               |
+------------------------------------------------------------------+
|                                                                  |
|  MAX_PROCESSES = 16                                              |
|                                                                  |
|  Index  PID  State     Name        Background  Description       |
|  -----  ---  -----     ----        ----------  -----------       |
|    0     0   RUNNING   [kernel]    No          Kernel process    |
|    1     1   RUNNING   [shell]     No          Shell process     |
|    2     2   RUNNING   sleep       Yes         Background task   |
|    3     -   FREE      -           -           Available slot    |
|    4     -   FREE      -           -           Available slot    |
|   ...   ...  ...       ...         ...         ...               |
|   15     -   FREE      -           -           Available slot    |
|                                                                  |
|  PCB Structure:                                                  |
|  +----------------------------------------------------------+    |
|  | pid           | Process ID (1-based, 0=kernel)            |   |
|  | parent_pid    | Parent process ID                         |   |
|  | name[32]      | Process name                              |   |
|  | cmdline[256]  | Full command line                         |   |
|  | state         | FREE/READY/RUNNING/BLOCKED/ZOMBIE         |   |
|  | exit_code     | Exit code (valid when ZOMBIE)             |   |
|  | background    | Running in background?                    |   |
|  | notified      | User notified of completion?              |   |
|  | start_time    | When process started (ms)                 |   |
|  | cpu_time      | Total CPU time used (ms)                  |   |
|  +----------------------------------------------------------+    |
|                                                                  |
|  Background Task Structure (single task):                        |
|  +----------------------------------------------------------+    |
|  | handler       | Command handler function                  |   |
|  | argc/argv     | Command arguments                         |   |
|  | pid           | Associated process ID                     |   |
|  | active        | Task is active?                           |   |
|  | is_nohup      | Output to nohup.out?                      |   |
|  | start_time    | When task started                         |   |
|  | target_end    | When sleep should end (for sleep cmd)     |   |
|  +----------------------------------------------------------+    |
|                                                                  |
+------------------------------------------------------------------+
```

---

## 11. Test Suite Structure

```
+------------------------------------------------------------------+
|                    Test Suite Structure                          |
+------------------------------------------------------------------+
|                                                                  |
|  tests/test_emberos.py                                           |
|         |                                                        |
|         v                                                        |
|  +------------------+                                            |
|  | EmberOSTest      |  Main test class                           |
|  +------------------+                                            |
|         |                                                        |
|         +---> start_vm()        Start QEMU with kernel           |
|         |         |                                              |
|         |         +---> pexpect.spawn(qemu-system-aarch64)       |
|         |         +---> Wait for "Starting shell"                |
|         |         +---> Wait for prompt "ember:/> "              |
|         |                                                        |
|         +---> run_all_tests()   Execute test categories          |
|         |         |                                              |
|         |         +---> test_basic_commands()                    |
|         |         +---> test_uartstat()                          |
|         |         +---> test_filesystem_basic()                  |
|         |         +---> test_write_and_cat()                     |
|         |         +---> test_cp_mv_comprehensive()               |
|         |         +---> test_xxd_hexdump()                       |
|         |         +---> test_wc_head_tail()                      |
|         |         +---> test_grep_find()                         |
|         |         +---> test_df_regs()                           |
|         |         +---> test_background_nohup()                  |
|         |         +---> test_nohup_output()                      |
|         |         +---> test_shell_features()                    |
|         |         +---> test_casm_hello()                        |
|         |         +---> test_casm_counter()                      |
|         |         +---> test_casm_math()                         |
|         |         +---> test_casm_memory()                       |
|         |         +---> test_casm_graphics()                     |
|         |         +---> test_casm_process()                      |
|         |         +---> test_casm_memtest()                      |
|         |         +---> test_casm_process_example()              |
|         |         +---> test_casm_compile_run()                  |
|         |         +---> test_casm_disasm()                       |
|         |         +---> test_casm_debug()                        |
|         |                                                        |
|         +---> stop_vm()         Shutdown QEMU                    |
|         |                                                        |
|         +---> print_summary()   Show results                     |
|                                                                  |
|  Helper Methods:                                                 |
|  +----------------------------------------------------------+    |
|  | send_command(cmd)    | Send command, wait for prompt      |   |
|  | _write_file(name,c)  | Write file using write command     |   |
|  | wait_for(pattern)    | Wait for regex pattern             |   |
|  | record_result(n,p,d) | Record test result                 |   |
|  +----------------------------------------------------------+    |
|                                                                  |
+------------------------------------------------------------------+
```
