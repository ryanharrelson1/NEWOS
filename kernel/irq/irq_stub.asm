
extern scheduler_tick
extern keyboard_handler


global timer_irq
timer_irq:
    cli 
    pusha 
    push ds 
    push es
    push fs 
    push gs 

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    lea eax,[esp+16]
    push eax
    call scheduler_tick
    add esp,4

    pop gs
    pop fs
    pop es
    pop ds
    popa

    mov al, 0x20
    out 0x20, al
  
    iret

global keyboard_irq

keyboard_irq:

     cli

    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10        ; kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call keyboard_handler

    pop gs
    pop fs
    pop es
    pop ds

    popa

    mov al, 0x20
    out 0x20, al       ; EOI

    iretd


