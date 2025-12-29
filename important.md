# ⚠️ Important Notice

This update introduces major architectural changes. While heavily tested (73 automated tests pass), some features may have edge cases that break.

## Known Vulnerable Areas

### 1. Background Tasks (nohup)
- Single CPU core means cooperative switching between foreground/background
- Only ONE background task at a time supported
- Fast task switching relies on shell idle loop - may miss timing
- Long-running foreground commands block background task progress

### 2. CASM Native Execution
- Programs run as native ARM64 code with SVC traps
- Reserved registers x24-x28 - using them WILL break execution
- Large programs (>5KB) will fail silently
- Some edge cases in extended opcodes may not handle errors gracefully

### 3. EL0/EL1 Exception Levels
- Shell runs in EL0, kernel in EL1 - separation is PARTIAL
- Some syscalls may not handle all error conditions
- Context switching between levels is complex - edge cases possible
- Memory protection not fully enforced

### 4. Interrupt-Driven UART
- Ring buffer (256 bytes) can overflow on rapid input
- WFI loop may occasionally miss characters under heavy load
- Statistics counters may wrap on very long sessions

## If Something Breaks

1. Try `reboot` command
2. If shell hangs, restart QEMU
3. Run `python3 tests/test_emberos.py` to verify system state
4. Check `uartstat` for UART errors/overflows

## Test Coverage

Run the automated test suite to verify functionality:
```bash
python3 tests/test_emberos.py
```

All 73 tests should pass. If tests fail, the system may be in an unstable state.
