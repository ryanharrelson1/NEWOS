#include <stdint.h>
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "isr/isr.h"
#include "irq/irq.h"
#include "irq/pic.h"
#include "drivers/serial.h"
#include "multiboot/memorymap.h"
#include "mem/paging.h"
#include "mem/kernel_heap.h"
#include "mem/slab.h"
#include "drivers/pit_timer.h"
#include "multitasking/proccess.h"
#include "gdt/tss.h"
#include "multitasking/scheduler.h"
#include "drivers/ide/ide.h"
#include "drivers/vga.h"



extern uint32_t  mb2_info_ptr;
extern uint32_t mb2_magic;


extern uint8_t _bin_user_start[];
extern uint8_t _bin_user_end[];


 






void kernel_main() {
    init_serial();
    gdt_install();
    idt_install();
    isr_install();
    pic_remap();
    irq_install();
    parse_memory_map(mb2_info_ptr);
    paging_init();
    kmalloc_init();
    pit_init();
    ide_init();
    vga_init();

    



    call_user_shell();

    














 __asm__ __volatile__("sti"); // Enable interrupts
    
    while (1) {
        __asm__ __volatile__("hlt");
    }
    
}


void call_user_shell() {
    uintptr_t user_code_size = (uintptr_t)_bin_user_end - (uintptr_t)_bin_user_start;


    process_t* user_proc = create_proccess((void*)_bin_user_start, user_code_size);
    


       serial_write_string("User process created.\n");
 serial_write_hex32(user_proc->kernelstack);

    if (!user_proc) {
        serial_write_string("Failed to create user process.\n");
        return;
    }




    serial_write_string("Starting user shell process...\n");

    scheduler_start();


     

  

}






















