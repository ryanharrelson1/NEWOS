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
#include "drivers/keyboard.h"
#include "isr/syscall.h"
#include "drivers/fs/vfs.h"





 



extern uint32_t  mb2_info_ptr; // provided by boot.s, points to the multiboot2 info structure passed by grub2
extern uint32_t mb2_magic; // provided by boot.s, should be 0x36d76289 for multiboot2 compliant bootloader (grub2)


extern uint8_t _bin_user_start[]; // provided by the linker script, marks the start of the embedded user shell binary in the kernel's address space
extern uint8_t _bin_user_end[]; // provided by the linker script, marks the end of the embedded user shell binary in the kernel's address space


// Here we will implement the kernel's main function, which will be called after the bootloader has loaded the kernel into memory and called boot.s
//  to setup paging and move kernel into high half of memory and setup the recursive mapping as well as map the vga mem to a high half adress and set up the
//  initial environment. This is where we will initialize all our subsystems and start the first user process.
void kernel_main() {
    init_serial(); // Initialize serial for early debugging output
    gdt_install(); // Setup Global Descriptor Table and TSS 
    idt_install(); // Setup Interrupt Descriptor Table
    isr_install(); // Setup ISRs for CPU exceptions
    pic_remap(); // Remap PIC to avoid conflicts with CPU exceptions
    irq_install(); // Setup IRQ handlers for hardware interrupts
    parse_memory_map(mb2_info_ptr); // Parse memory map provided by grub2 bootloader
    paging_init(); // clear the identity mapping in the low half of memory
    kmalloc_init(); // Initialize kernel heap allocator
    pit_init(); // Initialize Programmable Interval Timer for scheduling
    ide_init(); // Initialize IDE driver for disk access
    vga_init(); // Initialize VGA driver for text output
    keyboard_init(); // Initialize keyboard driver for input
    syscall_init(); // Initialize system call handlers
    vfs_mount_root(); // Mount the root filesystem (FAT16 on the boot disk)






   call_user_shell(); // Load and start the first user process (the shell)

    

 __asm__ __volatile__("sti"); // Enable interrupts
   // simple while loop to keep the kernel running. The scheduler will take over and switch to the user shell process.
   // we should never actually get here since the scheduler should switch to the user shell and never come back to this loop, but it's here as a fallback and to keep the shell from returning back to boot.s
    while (1) {
      
    }
    
}


void call_user_shell() {
    uintptr_t user_code_size = (uintptr_t)_bin_user_end - (uintptr_t)_bin_user_start;


    process_t* user_proc = create_proccess((void*)_bin_user_start, user_code_size);
    


       serial_write_string("User process created.\n");
        serial_write_hex32(user_proc->kernelstack);
        //asm volatile("hlt");

    if (!user_proc) {
        serial_write_string("Failed to create user process.\n");
        return;
    }




    serial_write_string("Starting user shell process...\n");

    scheduler_start();

}

























