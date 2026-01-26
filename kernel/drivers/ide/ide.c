#include "ide.h"
#include "../serial.h"
#include "ide_io.h"
\

ide_device_t* found_devices[4];
int num_found_devices = 0;

 ide_channels_t primary = {IDE_PRIME_BASE, IDE_PRIME_CTRL, 14};
 ide_channels_t secondary = {IDE_SECOND_BASE,IDE_SECOND_CTRL, 15};

ide_device_t devices[4] = {
    {&primary, 0, "", 0, 0},
    {&primary, 1, "", 0, 0},
    {&secondary, 0, "", 0, 0},
    {&secondary, 1, "", 0, 0}
};


void ide_wait_bsy_clear(ide_channels_t* ch) {
    // Wait until BSY = 0
    while (ide_inb(ch->base + IDE_STATUS) & 0x80);
}

void ide_wait_drq(ide_channels_t* ch) {
    // Wait until DRQ = 1
    while (!(ide_inb(ch->base + IDE_STATUS) & 0x08));
}

int ide_ident(ide_device_t* dev) {
    ide_outb(dev->channel->base + 6, 0xA0 | (dev->drive << 4));
    ide_outb(dev->channel->base + 7, IDE_CMD_IDENT);

    uint8_t status = ide_inb(dev->channel->base + 7);
    if(status == 0) return 0;


    ide_wait_ready(dev->channel);


    uint16_t data[256];

    for(int i=0; i<256; i++)
    data[i] = ide_inw(dev->channel->base);


    for(int i = 0; i<40; i+=2){
        dev->model[i]  = data[27 + i/2] >> 8;
        dev->model[i+1] = data[27 + i/2] & 0xFF;
    }
    dev->model[40] = 0;

    dev->sectors = ((uint32_t)data[61] << 16) | data[60];

    return 1;
}


void ide_init() {
    num_found_devices = 0;

    for(int i=0; i<4; i++){
        if(ide_ident(&devices[i])) {
            devices[i].present = 1;
             found_devices[num_found_devices++] = &devices[i];
        } else {
            devices[i].present = 0;
         
        }
    }
}

void ide_wait_device_ready(ide_channels_t* channel) {
    while (ide_inb(channel->base + IDE_STATUS) & IDE_SR_BSY);
    while (!(ide_inb(channel->base + IDE_STATUS) & IDE_SR_DRQ));
}

void ide_read_sector(ide_device_t* dev, uint32_t lba, uint8_t* buf){
    ide_channels_t* ch = dev->channel;


    ide_outb(ch->base + IDE_DRIVE_SEL, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ide_outb(ch->base + IDE_SECCOUNT, 1);
    ide_outb(ch->base + IDE_LBA_LOW, lba & 0xFF);
    ide_outb(ch->base + IDE_LBA_MID, (lba >> 8) & 0xFF);
    ide_outb(ch->base + IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    ide_outb(ch->base + IDE_COMMAND,IDE_CMD_READ_SECTOR);

    ide_wait_ready(ch);


    // read 512 

    for(int i = 0; i < 256; i++) {
        uint16_t w = ide_inw(ch->base + IDE_DATA);
        buf[i*2] = w & 0xFF;
        buf[i*2 + 1] = w >> 8;
    }
    
}


void ide_write_sector(ide_device_t* dev, uint32_t lba, uint8_t* buf) {
    ide_channels_t* ch = dev->channel;

    // Select drive
    ide_outb(ch->base + IDE_DRIVE_SEL, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    // 400ns delay
    for (int i = 0; i < 4; i++) ide_inb(ch->ctrl);

    ide_outb(ch->base + IDE_SECCOUNT, 1);
    ide_outb(ch->base + IDE_LBA_LOW, lba & 0xFF);
    ide_outb(ch->base + IDE_LBA_MID, (lba >> 8) & 0xFF);
    ide_outb(ch->base + IDE_LBA_HIGH, (lba >> 16) & 0xFF);
    ide_outb(ch->base + IDE_COMMAND, IDE_CMD_WRITE_SECTOR);

    ide_wait_bsy_clear(ch);
    ide_wait_drq(ch);

    for (int i = 0; i < 256; i++) {
        uint16_t w = buf[i*2] | (buf[i*2 + 1] << 8);
        ide_outw(ch->base + IDE_DATA, w);
    }

    // Flush cache
    ide_outb(ch->base + IDE_COMMAND, 0xE7);
    ide_wait_bsy_clear(ch);

    // Optional: check ERR bit
    uint8_t status = ide_inb(ch->base + IDE_STATUS);
    if (status & 0x01) {
        serial_write_string("IDE Write Error!\n");
    }
}