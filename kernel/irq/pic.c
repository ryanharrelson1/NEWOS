#include "pic.h"
#include "../io/io.h"  // for outb and inb functions

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2+1)
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

void pic_remap() {
    uint8_t a1, a2;

    a1 = inb(PIC1_DATA); // save masks
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT);
    outb(PIC2_COMMAND, ICW1_INIT);
    outb(PIC1_DATA, 0x20); // PIC1 vector offset = 32
    outb(PIC2_DATA, 0x28); // PIC2 vector offset = 40
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA,  0xFC); // restore masks
    outb(PIC2_DATA, 0xFF);
}


void send_timer_eoi() {
    outb(0x20, 0x20); // PIC1, IRQ0 (timer)
}