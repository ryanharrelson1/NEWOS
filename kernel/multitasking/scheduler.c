#include "scheduler.h"
#include "proccess.h"




process_t *current_process = 0;

process_t* process_list = NULL;
#define PAGE_PRESENT 0x1



void scheduler_tick(uintptr_t* stack_frame) {
    if (!current_process) return;

    process_t* prev = current_process;

    /* Save context */
    prev->context.eax = stack_frame[7];
    prev->context.ecx = stack_frame[6];
    prev->context.edx = stack_frame[5];
    prev->context.ebx = stack_frame[4];
    prev->context.ebp = stack_frame[2];
    prev->context.esi = stack_frame[1];
    prev->context.edi = stack_frame[0];
    prev->context.eip = stack_frame[8];
    prev->context.cs  = stack_frame[9];
    prev->context.eflags = stack_frame[10];
    prev->context.useresp = stack_frame[11];
    prev->context.ss = stack_frame[12];

    if (prev->state == TASK_RUNNING)
        prev->state = TASK_READY;

    process_t* next = prev->next;
    while (next != prev && next->state != TASK_READY)
        next = next->next;

    if (next == prev || next->state != TASK_READY) {
        prev->state = TASK_RUNNING;
        return;
    }


    /* Switch address space FIRST */
    cpu_load_cr3((uintptr_t)next->page_directory);

   


    current_process = next;
    current_process->state = TASK_RUNNING;

    /* Restore context */
    stack_frame[7] = next->context.eax;
    stack_frame[6] = next->context.ecx;
    stack_frame[5] = next->context.edx;
    stack_frame[4] = next->context.ebx;
    stack_frame[2] = next->context.ebp;
    stack_frame[1] = next->context.esi;
    stack_frame[0] = next->context.edi;

    stack_frame[8]  = next->context.eip;
    stack_frame[9]  = next->context.cs;
    stack_frame[10] = next->context.eflags;
    stack_frame[11] = next->context.useresp;
    stack_frame[12] = next->context.ss;

  
}


void scheduler_start() {

   if (!current_process) return;
   

   
    cpu_load_cr3((uintptr_t)current_process->page_directory);
    set_kernel_stack(current_process->kernelstack);
      
   
   
    current_process->state = TASK_RUNNING;
 
    uintptr_t* stack = (uintptr_t*)(current_process->kernelstack);
    
    




    *(--stack) = current_process->context.ss;       // SS
    *(--stack) = current_process->context.useresp;  // ESP
    *(--stack) = current_process->context.eflags;   // EFLAGS
    *(--stack) = current_process->context.cs;       // CS
    *(--stack) = current_process->context.eip;      // EIP


    serial_write_string("hello world");
    serial_write_hex32(current_process->context.useresp);
    serial_write_string("\n");
    serial_write_hex32(current_process->context.eip);

    

  


     
   asm volatile (
    "cli\n"                          // Disable interrupts
    "mov $0x23, %%ax\n"
    "mov %%ax, %%ds\n"
    "mov %%ax, %%es\n"
    "mov %%ax, %%fs\n"
    "mov %%ax, %%gs\n"
    "mov %0, %%esp\n"
    "iret\n"
    : : "r"(stack) : "memory", "ax"
);

    __builtin_unreachable();
}



void add_process(process_t* proc) {
    if (!process_list) {
        process_list = proc;
        proc->next = proc; // circular
        current_process = proc;
    } else {
        // Insert at the end
        process_t* tmp = process_list;
        while (tmp->next != process_list)
            tmp = tmp->next;
        tmp->next = proc;
        proc->next = process_list;
    }
}



