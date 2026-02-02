#include "irq.h"
#include "../idt/idt.h"

extern void timer_irq(void);

extern void tss_test_isr(void);
extern void keyboard_irq(void);

void irq_install(void) {
    idt_set_gate(32, (uint32_t)timer_irq, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_irq, 0x08, 0x8E);

}