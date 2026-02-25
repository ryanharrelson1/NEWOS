#include "ide.h"
#include "../serial.h"
#include "ide_io.h"


ide_device_t* found_devices[4];
int num_found_devices = 0;
ide_device_t* ide_boot_disk = 0;

 ide_channels_t primary = {IDE_PRIME_BASE, IDE_PRIME_CTRL, 14};
 ide_channels_t secondary = {IDE_SECOND_BASE,IDE_SECOND_CTRL, 15};

ide_device_t devices[4] = {
    {&primary, 0, "", 0, 0},
    {&primary, 1, "", 0, 0},
    {&secondary, 0, "", 0, 0},
    {&secondary, 1, "", 0, 0}
};

static inline void ide_400ns_delay(ide_channels_t* ch) {
    // ALTSTATUS is typically read from control base + 0 (or ch->ctrl if you store that)
    // You already use ch->ctrl in write, so keep it consistent:
    for (int i = 0; i < 4; i++) ide_inb(ch->ctrl);
}

static int ide_pio_wait_data(ide_channels_t* ch) {
    // 400ns delay then poll
    ide_400ns_delay(ch);

    for (uint32_t t = 0; t < 1000000; t++) {
        uint8_t st = ide_inb(ch->base + IDE_STATUS);

        if (st & IDE_SR_ERR) return -1;
        if (st & IDE_SR_DF)  return -2;

        // BSY cleared and DRQ set => data ready
        if (!(st & IDE_SR_BSY) && (st & IDE_SR_DRQ)) return 0;

        if (t == 999999) return -4;
    }
    return -4;
}


static int ide_poll_drq(ide_channels_t* ch) {
    // Wait BSY clear with timeout
    for (uint32_t t = 0; t < 1000000; t++) {
        if (!(ide_inb(ch->base + IDE_STATUS) & IDE_SR_BSY)) break;
        if (t == 999999) return -3;
    }

    // Wait DRQ set or error with timeout
    for (uint32_t t = 0; t < 1000000; t++) {
        uint8_t st = ide_inb(ch->base + IDE_STATUS);
        if (st & IDE_SR_ERR) return -1;
        if (st & IDE_SR_DF)  return -2;
        if (st & IDE_SR_DRQ) return 0;
        if (t == 999999) return -4;
    }
    return -4;
}

static int ide_wait_not_busy_check_err(ide_channels_t* ch) {
    for (uint32_t t = 0; t < 1000000; t++) {
        uint8_t st = ide_inb(ch->base + IDE_STATUS);
        if (!(st & IDE_SR_BSY)) {
            if (st & IDE_SR_ERR) return -1;
            if (st & IDE_SR_DF)  return -2;
            return 0;
        }
        if (t == 999999) return -3;
    }
    return -3;
}

int ide_ident(ide_device_t* dev) {
    ide_channels_t* ch = dev->channel;

    ide_outb(ch->base + IDE_DRIVE_SEL, 0xA0 | (dev->drive << 4));
    ide_400ns_delay(ch);

    ide_outb(ch->base + IDE_SECCOUNT, 0);
    ide_outb(ch->base + IDE_LBA_LOW,  0);
    ide_outb(ch->base + IDE_LBA_MID,  0);
    ide_outb(ch->base + IDE_LBA_HIGH, 0);

    ide_outb(ch->base + IDE_COMMAND, IDE_CMD_IDENT); // 0xEC
    ide_400ns_delay(ch);

    uint8_t status = ide_inb(ch->base + IDE_STATUS);
    if (status == 0) return 0; // no device

    // If ERR set, it might be ATAPI. Check signature.
    if (status & IDE_SR_ERR) {
        uint8_t mid  = ide_inb(ch->base + IDE_LBA_MID);
        uint8_t high = ide_inb(ch->base + IDE_LBA_HIGH);

        // ATAPI signatures (common)
        if ((mid == 0x14 && high == 0xEB) || (mid == 0x69 && high == 0x96)) {
            return 0; // skip ATAPI devices for now (CD-ROM)
        }
        return 0;
    }

    int pr = ide_poll_drq(ch);
    if (pr != 0) return 0;

    uint16_t data[256];
    for (int i = 0; i < 256; i++) data[i] = ide_inw(ch->base + IDE_DATA);

    for (int i = 0; i < 40; i += 2) {
        dev->model[i]   = data[27 + i/2] >> 8;
        dev->model[i+1] = data[27 + i/2] & 0xFF;
    }
    dev->model[40] = 0;

    dev->sectors = ((uint32_t)data[61] << 16) | data[60];

    // If sectors==0, it’s not a normal HDD (or identify failed weirdly). Skip it.
    if (dev->sectors == 0) return 0;

    return 1;
}


void ide_init() {
    num_found_devices = 0;
    ide_boot_disk = 0;

    for (int i = 0; i < 4; i++) {
        if (ide_ident(&devices[i])) {
            devices[i].present = 1;
            found_devices[num_found_devices++] = &devices[i];

            if (!ide_boot_disk) ide_boot_disk = &devices[i]; // first found
        } else {
            devices[i].present = 0;
        }
    }
    serial_write_string("IDE found: ");
serial_write_hex32(num_found_devices);
serial_write_string("\n");

if (ide_boot_disk) {
    serial_write_string("boot disk model: ");
    serial_write_string(ide_boot_disk->model);
    serial_write_string("\nsectors: ");
    serial_write_hex32(ide_boot_disk->sectors);
    serial_write_string("\n");
}

}


int ide_read_sectors(ide_device_t* dev, uint32_t lba, uint8_t count, uint8_t* buf) {
    ide_channels_t* ch = dev->channel;
    uint32_t n = (count == 0) ? 256u : (uint32_t)count;

    ide_outb(ch->base + IDE_DRIVE_SEL,
             0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ide_400ns_delay(ch);

    int rr = ide_wait_not_busy_check_err(ch);
    if (rr) return rr;

    ide_outb(ch->base + IDE_SECCOUNT, count);
    ide_outb(ch->base + IDE_LBA_LOW,  (uint8_t)(lba & 0xFF));
    ide_outb(ch->base + IDE_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    ide_outb(ch->base + IDE_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));

    ide_outb(ch->base + IDE_COMMAND, IDE_CMD_READ_SECTOR);
    ide_400ns_delay(ch);
    (void)ide_inb(ch->base + IDE_STATUS);

    for (uint32_t s = 0; s < n; s++) {
        int pr = ide_pio_wait_data(ch);
        if (pr != 0) return pr;

        uint16_t* outw = (uint16_t*)(buf + s * 512);
        for (int i = 0; i < 256; i++) {
            outw[i] = ide_inw(ch->base + IDE_DATA);
        }
        (void)ide_inb(ch->base + IDE_STATUS);
    }

    return 0;
}



int ide_write_sectors(ide_device_t* dev, uint32_t lba, uint8_t count, const uint8_t* buf) {
    ide_channels_t* ch = dev->channel;
    uint32_t n = (count == 0) ? 256u : (uint32_t)count;

    ide_outb(ch->base + IDE_DRIVE_SEL,
             0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ide_400ns_delay(ch);

    ide_outb(ch->base + IDE_SECCOUNT, count);
    ide_outb(ch->base + IDE_LBA_LOW,  (uint8_t)(lba & 0xFF));
    ide_outb(ch->base + IDE_LBA_MID,  (uint8_t)((lba >> 8) & 0xFF));
    ide_outb(ch->base + IDE_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    ide_outb(ch->base + IDE_COMMAND, IDE_CMD_WRITE_SECTOR); // 0x30

    for (uint32_t s = 0; s < n; s++) {
        int pr = ide_poll_drq(ch);
        if (pr != 0) return pr;

        const uint16_t* inw = (const uint16_t*)(buf + s * 512);
        for (int i = 0; i < 256; i++) {
            ide_outw(ch->base + IDE_DATA, inw[i]);
        }
    }

    // Flush cache
    ide_outb(ch->base + IDE_COMMAND, 0xE7);
    return ide_wait_not_busy_check_err(ch);
}

