#include "tss.h"
#include "gdt.h"





struct tss_entry_t tss_entry;

extern void tss_flush();



void write_tss(int gdt_index, uint32_t kernel_ss, uint32_t kernel_esp){

     for(uint32_t i = 0; i < sizeof(tss_entry); i++) {
        ((uint8_t*)&tss_entry)[i] = 0;
    }



    tss_entry.ss0 = kernel_ss;
    tss_entry.esp0 = kernel_esp;
    tss_entry.cs = 0x08;
    tss_entry.ss = 0x10;

    tss_entry.iomap_base = sizeof(tss_entry);

    uint32_t base = (uint32_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry) - 1;

    gdt_set_gate(gdt_index, base, limit, 0x89, 0x00);


}



void set_kernel_stack(uint32_t stack){
    tss_entry.esp0 = stack;
      tss_entry.ss0 = 0x10; // Kernel data segment
}


void test_tss_sanity() {
    serial_write_string("=== TSS SANITY CHECK ===\n");

    serial_write_string("tss_entry.ss0: 0x");
    serial_write_hex32(tss_entry.ss0);
    serial_write_string("\n");

    serial_write_string("tss_entry.esp0: 0x");
    serial_write_hex32(tss_entry.esp0);
    serial_write_string("\n");

    serial_write_string("tss_entry.iomap_base: 0x");
    serial_write_hex32(tss_entry.iomap_base);
    serial_write_string("\n");

    // Optional: dump first 16 bytes of the kernel stack
    serial_write_string("Kernel stack top (first 16 bytes):\n");
    uint32_t* kstack = (uint32_t*)tss_entry.esp0;
    for (int i = 0; i < 4; i++) {
        serial_write_hex32(kstack[i]);
        serial_write_string(" ");
    }
    serial_write_string("\n");

    serial_write_string("=======================\n");
}