ENTRY(_start)

SECTIONS {
    . = 0x00100000;

    .multiboot2 ALIGN(8) : AT(ADDR(.multiboot2)) {
        KEEP(*(.multiboot2))
    }

    .boot ALIGN(4K) : AT(ADDR(.boot)) {
        KEEP(*(.boot))
    }

    .userprog ALIGN(4K) : AT(ADDR(.userprog)) {
        _bin_user_start = .;
        KEEP(*(.userprog))
        _bin_user_end = .;
    }


    /* LMA for .text */
    __text_lma_start = 0x00110000;

    . = 0xC0000000;

    .text ALIGN(4K) : AT(__text_lma_start) {

        *(.text*)
        __text_lma_end = LOADADDR(.text) + SIZEOF(.text);
    }

    /* LMA for .rodata aligned after .text */
    __rodata_lma_start = (__text_lma_end + 0xFFF) & ~0xFFF;

    .rodata ALIGN(4K) : AT(__rodata_lma_start) {
        *(.rodata*)
        *(.eh_frame)
        __rodata_lma_end = LOADADDR(.rodata) + SIZEOF(.rodata);
    }

    /* LMA for .data aligned after .rodata */
    __data_lma_start = (__rodata_lma_end + 0xFFF) & ~0xFFF;

    .data ALIGN(4K) : AT(__data_lma_start) {
        *(.data*)
        *(.got*)
        *(.got.plt*)
        __data_lma_end = LOADADDR(.data) + SIZEOF(.data);
    }

    /* LMA for .bss aligned after .data */
    __bss_lma_start = (__data_lma_end + 0xFFF) & ~0xFFF;

    .bss ALIGN(4K) : AT(__bss_lma_start) {
        *(.bss*)
        *(COMMON)
    }


     _kernel_end_phys = (__bss_lma_start + SIZEOF(.bss));

}