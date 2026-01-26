#include "ide.h"
#include "../../io/io.h"

// port command helpers to comm with the ide controller on the chipset


uint8_t ide_inb(uint16_t port) {return inb(port);}
void ide_outb(uint16_t port, uint8_t val) {outb(port, val);}
uint16_t ide_inw(uint16_t port) {return inw(port);}
void ide_outw(uint16_t port, uint16_t val) {outw(port, val);}

void ide_wait_ready(ide_channels_t* channel){
    while(ide_inb(channel->base + 7) & IDE_SR_BSY);
    while(!(ide_inb(channel->base + 7) & IDE_SR_DRDY));
}