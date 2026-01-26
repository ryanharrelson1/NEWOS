
#include <stdint.h>
#include "../drivers/serial.h"
#include "../multitasking/proccess.h"

extern void manual_switch(process_t* next, uintptr_t* frame);






void isr_handler(uint32_t int_no, uint32_t err_code) {
    // Halt on CPU exceptions
    if (int_no < 32) {
        // For now, just halt the CPU
        serial_write_string("CPU Exception occurred! Halting.\n");
        __asm__ __volatile__("cli; hlt");
    } else {
        // Future: IRQs
    }
}




struct regs {
    // Segment registers (pushed manually first in stub)
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;

    // General-purpose registers (pusha)
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;     // original esp before pusha
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t int_no;   // software pushed interrupt number
    uint32_t err_code; // CPU pushes for GPF

    // CPU-pushed values
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t useresp;  // only if privilege change
    uint32_t ss;       // only if privilege change
};



void gpf_handler(struct regs *r, uint32_t err_code) {
    serial_write_string("=== GENERAL PROTECTION FAULT ===\n");
    serial_write_string("CS:EIP = ");
    serial_write_hex32(r->cs);
    serial_write_string(":");
    serial_write_hex32(r->eip);
    serial_write_string("\n");
    serial_write_string("Error code: ");
    serial_write_hex32(r->err_code);
    serial_write_string("\n");

    serial_write_string("ESP: ");
    serial_write_hex32(r->useresp);
    serial_write_string("\n");
    serial_write_hex32(r->ss);
    serial_write_string("\n");

    serial_write_string("Fault in ");
    if (r->cs & 0x3)
        serial_write_string("USER mode\n");
    else
        serial_write_string("KERNEL mode\n");

    while(1); // stop
}



void syscall_handler_c(uintptr_t* stack_frame) {
    serial_write_string("Syscall: trapped into kernel!\n");


}


void tss_handler(){
    serial_write_string("TSS Exception: Invalid TSS!\n");
    while(1); // halt
}


void seg_fault_handler(){
    serial_write_string("Segment Fault Exception!\n");
    while(1); // halt
}


void stack_fault_handler(){
    serial_write_string("Stack Fault Exception!\n");
    while(1); // halt
}

void opcode_fault_handler(){
    serial_write_string("Opcode Fault Exception: Invalid Opcode!\n");
    while(1); // halt
}