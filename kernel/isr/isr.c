#include "isr.h"
#include "../idt/idt.h"

extern void isr_generic_exception_stub(void);
extern void isr_gp_handler_stub(void);
extern void isr_syscall_stub(void);
extern void isr_tss_handler_stub(void);
extern void isr_segment_fault_handler_stub(void);
extern void isr_stack_fault_handler_stub(void);
extern void isr_opcode_fault_handler_stub(void);
#define SYSCALL_VECTOR 0x80



void isr_install(void) {


    for (int i = 0; i < 32; i++) {

        if(i != 13 && i != SYSCALL_VECTOR && i != 10 && i != 11 && i !=12 && i !=6)
        idt_set_gate(i, (uint32_t)isr_generic_exception_stub, 0x08, 0x8E);
    }

    idt_set_gate(13, (uint32_t)isr_gp_handler_stub, 0x08, 0x8E);

     idt_set_gate(SYSCALL_VECTOR, (uint32_t)isr_syscall_stub, 0x08, 0x8E | 0x60);

     idt_set_gate(10, (uint32_t)isr_tss_handler_stub, 0x08, 0x8E);
      idt_set_gate(11, (uint32_t)isr_segment_fault_handler_stub, 0x08, 0x8E);
      idt_set_gate(12, (uint32_t)isr_stack_fault_handler_stub, 0x08, 0x8E);
      idt_set_gate(12, (uint32_t)isr_stack_fault_handler_stub, 0x08, 0x8E);
      idt_set_gate(6, (uint32_t)isr_opcode_fault_handler_stub, 0x08, 0x8E);

}