BITS 32
extern isr_handler
global isr_gp_handler_stub
extern gpf_handler




global isr_generic_exception_stub
isr_generic_exception_stub:
    cli
    push dword 0              
    push dword 0          
    call isr_handler
    add esp, 8
    sti
    iret



isr_gp_handler_stub:
    cli
    pusha               ; eax, ecx, edx, ebx, esp, ebp, esi, edi
    push ds
    push es
    push fs
    push gs
    push 0              ; error code (if CPU doesn't push it)
    push esp            ; pointer to struct regs
    call gpf_handler
    hlt

global isr_syscall_stub
extern syscall_dispatch

isr_syscall_stub:

     cli                     ; clear interrupts
    pusha                  ; push all general-purpose registers
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10            ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; pass pointer to stack frame to C
    call syscall_dispatch
    add esp, 4              ; clean argument

    pop gs
    pop fs
    pop es
    pop ds
    popa
    sti
    iret



global isr_tss_handler_stub
extern tss_handler
isr_tss_handler_stub:
    cli
    push dword 0              
    push dword 0          
    call tss_handler
    add esp, 8
    sti
    iret

global isr_segment_fault_handler_stub
extern seg_fault_handler

isr_segment_fault_handler_stub:
    cli
    push dword 0              
    push dword 0          
    call seg_fault_handler
    add esp, 8
    sti
    iret


global isr_stack_fault_handler_stub
extern stack_fault_handler 

isr_stack_fault_handler_stub:

     cli
    push dword 0              
    push dword 0          
    call stack_fault_handler
    add esp, 8
    sti
    iret


global isr_opcode_fault_handler_stub
extern opcode_fault_handler 

isr_opcode_fault_handler_stub:

    cli
    push dword 0              
    push dword 0          
    call opcode_fault_handler
    add esp, 8
    sti
    iret