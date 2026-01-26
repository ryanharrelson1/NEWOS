#ifndef GDT_H
#define GDT_H


#include <stdint.h>



struct __attribute__((packed)) gdt_entry_t
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct __attribute__((packed)) gdt_ptr_t
{
    uint16_t limit;
    uint32_t base;
};


void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
void gdt_install(void);

extern struct gdt_entry_t gdt_entries[6];
extern struct gdt_ptr_t gdt_ptr;

#endif // GDT_H