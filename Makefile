# EmberOS Makefile
# Target: ARM64 (AArch64) - QEMU virt machine

# Cross-compiler toolchain
# Try aarch64-elf- (homebrew), aarch64-none-elf-, or aarch64-linux-gnu-
CROSS_COMPILE ?= aarch64-elf-
CC      = $(CROSS_COMPILE)gcc
CXX     = $(CROSS_COMPILE)g++
AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# Directories
SRC_DIR     = src
KERNEL_DIR  = $(SRC_DIR)/kernel
DRIVERS_DIR = $(SRC_DIR)/drivers
SHELL_DIR   = $(SRC_DIR)/shell
CASM_DIR    = $(SRC_DIR)/casm
INC_DIR     = include
BUILD_DIR   = build

# Output
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
LINKER_SCRIPT = kernel.ld

# Compiler flags
CFLAGS = -Wall -Wextra -ffreestanding -nostdlib -nostartfiles \
         -mcpu=cortex-a53 -mgeneral-regs-only \
         -I$(INC_DIR) -O2 -g

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

ASFLAGS = -mcpu=cortex-a53

LDFLAGS = -nostdlib -T $(LINKER_SCRIPT)

# Source files
ASM_SRCS = $(shell find $(SRC_DIR) -name '*.S' 2>/dev/null)
C_SRCS   = $(shell find $(SRC_DIR) -name '*.c' 2>/dev/null)
CXX_SRCS = $(shell find $(SRC_DIR) -name '*.cpp' 2>/dev/null)

# Object files
ASM_OBJS = $(patsubst $(SRC_DIR)/%.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS   = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
CXX_OBJS = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CXX_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS) $(CXX_OBJS)

# QEMU settings
QEMU = qemu-system-aarch64
QEMU_FLAGS = -M virt -cpu cortex-a53 -m 128M -nographic \
             -kernel $(KERNEL_ELF)

# Default target
all: $(KERNEL_ELF)

# Link kernel ELF
$(KERNEL_ELF): $(OBJS) $(LINKER_SCRIPT) | $(BUILD_DIR)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "  OBJDUMP $@.dump"
	@$(OBJDUMP) -d $@ > $@.dump

# Create binary image
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "  OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@

# Compile assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Run in QEMU
run: $(KERNEL_ELF)
	@echo "Starting QEMU..."
	@echo "Press Ctrl+A, X to exit"
	$(QEMU) $(QEMU_FLAGS)

# Run with GDB server
debug: $(KERNEL_ELF)
	@echo "Starting QEMU with GDB server on port 1234..."
	$(QEMU) $(QEMU_FLAGS) -S -s

# Clean build artifacts
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR)

# Show build info
info:
	@echo "Cross-compiler: $(CROSS_COMPILE)"
	@echo "ASM sources:    $(ASM_SRCS)"
	@echo "C sources:      $(C_SRCS)"
	@echo "C++ sources:    $(CXX_SRCS)"
	@echo "Objects:        $(OBJS)"

.PHONY: all clean run debug info
