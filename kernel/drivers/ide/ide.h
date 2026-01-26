#pragma once
#ifndef IDE_H
#define IDE_H
#include <stdint.h>

#define IDE_DATA       0x00   // Data port (16-bit)
#define IDE_ERROR      0x01   // Error (read)
#define IDE_FEATURES   0x01   // Features (write)
#define IDE_SECCOUNT   0x02   // Sector count
#define IDE_LBA_LOW    0x03   // LBA low byte
#define IDE_LBA_MID    0x04   // LBA mid byte
#define IDE_LBA_HIGH   0x05   // LBA high byte
#define IDE_DRIVE_SEL  0x06   // Drive/head
#define IDE_STATUS     0x07   // Status (read)
#define IDE_COMMAND    0x07   // Command (write)
#define IDE_ALT_STATUS 0x206  // Alternate status (read)
#define IDE_CONTROL    0x206  // Control (write)


#define IDE_PRIME_BASE 0x1F0
#define IDE_SECOND_BASE 0x170
#define IDE_PRIME_CTRL 0x3F6
#define IDE_SECOND_CTRL 0x376


// status control points

#define IDE_SR_BSY 0x80
#define IDE_SR_DRDY 0x40
#define IDE_SR_DRQ 0x08


//commands
#define IDE_CMD_IDENT 0xEC
#define IDE_CMD_READ_SECTOR 0x20
#define IDE_CMD_WRITE_SECTOR 0x30

typedef struct {
    uint16_t base;
    uint16_t ctrl;
    uint8_t irq;
} ide_channels_t;

typedef struct {
    ide_channels_t* channel;
    uint8_t drive;
    char model[41];
    uint32_t sectors;
    uint8_t present;
} ide_device_t;

extern ide_device_t devices[4];

void ide_init();

int ide_ident(ide_device_t* dev);

void ide_wait_device_ready(ide_channels_t* channel);

void ide_read_sector(ide_device_t* dev, uint32_t lba, uint8_t* buf);
void ide_write_sector(ide_device_t* dev , uint32_t lba, uint8_t* buf);

#endif