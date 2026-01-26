#pragma once 
#ifndef IDE_IO_H
#define IDE_IO_H



uint8_t ide_inb(uint16_t port);

void ide_outb(uint16_t port, uint8_t val);

uint16_t ide_inw(uint16_t port);

void ide_outw(uint16_t port, uint16_t val);

void ide_wait_ready(ide_channels_t* channel);

#endif