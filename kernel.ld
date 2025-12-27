/*
 * EmberOS Kernel Linker Script
 * Target: ARM64 (AArch64) - QEMU virt machine
 */

ENTRY(_start)

/* Kernel load address for QEMU virt machine */
KERNEL_BASE = 0x40000000;

/* Stack size: 256KB (needed for CASM assembler) */
STACK_SIZE = 0x40000;

SECTIONS
{
    /* Start at kernel base address */
    . = KERNEL_BASE;

    /* Code section */
    .text : {
        __text_start = .;
        *(.text.boot)       /* Boot code first */
        *(.text .text.*)    /* All other code */
        __text_end = .;
    }

    /* Read-only data */
    .rodata : ALIGN(4096) {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    /* Initialized data */
    .data : ALIGN(4096) {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    /* Uninitialized data (BSS) */
    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Align to page boundary after BSS */
    . = ALIGN(4096);

    /* Stack grows downward, so we place it after BSS */
    __stack_bottom = .;
    . = . + STACK_SIZE;
    __stack_top = .;

    /* End of kernel image */
    __kernel_end = .;

    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}

/* Provide symbols for C/C++ code */
PROVIDE(__bss_size = __bss_end - __bss_start);
