#ifndef IDT_H
#define IDT_H


#include <stdint.h>


 struct idt_entry_t
{
    uint16_t base_low;
    uint16_t sel;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));


struct idt_ptr_t
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));





void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);


void idt_install();


#endif // IDT_H