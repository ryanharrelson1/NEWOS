#include "gdt.h"
#include "tss.h"
#include "../drivers/serial.h"

extern void gdt_flush(uint32_t);
extern void tss_flush(void);



struct gdt_entry_t gdt_entries[6];
struct gdt_ptr_t gdt_ptr;
extern char stack_top[];



void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) 
{
  gdt_entries[num].base_low =(base & 0xFFFF);
  gdt_entries[num].base_middle = (base >> 16) & 0xFF;
  gdt_entries[num].base_high = (base >> 24) & 0xFF;

  gdt_entries[num].limit_low = (limit & 0xFFFF);
  gdt_entries[num].granularity = ((limit >> 16) & 0x0F);

  gdt_entries[num].granularity |= (gran & 0xF0);
  gdt_entries[num].access = access;

}



void gdt_install(void)
{
 gdt_ptr.limit = (sizeof(struct gdt_entry_t) * 6) - 1;
 gdt_ptr.base = (uint32_t)&gdt_entries;

 gdt_set_gate(0, 0, 0, 0, 0);

 gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

 gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

 gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

 gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

 
 
  
  write_tss(5, 0x10, (uint32_t)stack_top); // Write TSS to GDT

 gdt_flush((uint32_t)&gdt_ptr);

 tss_flush(); // Flush TSS to load it


}