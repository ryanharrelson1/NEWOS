#include "pit_timer.h"
#include "../io/io.h"


void pit_init() {
    uint32_t divisor = 1193182 / PIT_FREQUENCY;

    outb(0x43, 0x36); // Command byte

    outb(0x40, divisor & 0xFF);       // Low byte
    outb(0x40, (divisor >> 8) & 0xFF); // High byte
}