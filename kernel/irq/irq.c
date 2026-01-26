#include "irq.h"
#include "../idt/idt.h"

extern void timer_irq(void);

extern void tss_test_isr(void);

void irq_install(void) {
    idt_set_gate(32, (uint32_t)timer_irq, 0x08, 0x8E);

}