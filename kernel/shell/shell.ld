ENTRY(_starts)

SECTIONS
{

    . = 0x00400000;


    .text : ALIGN(4K)
    {
        *(.text)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    .data : ALIGN(4K)
    {
        *(.data)
    }

    .bss : ALIGN(4K)
    {
        *(.bss)
    }

    /DISCARD/ : 
    {
        *(.eh_frame)
        *(.comment)
    }

}