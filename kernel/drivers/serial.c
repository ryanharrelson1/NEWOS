#include "serial.h"
#include "../io/io.h"




#define SERIAL_PORT 0x3F8
#define SERIAL_LCR   0x03   
#define SERIAL_FCR   0x01   
#define SERIAL_DLL   0x00  
#define SERIAL_DLM   0x00   
#define SERIAL_IER   0x00 
#define SERIAL_LSR   0x05

static void wait_for_transmit(){
    while ((inb(SERIAL_PORT + SERIAL_LSR) & 0x20) == 0);
}


void init_serial() {
    outb(SERIAL_PORT + 1, 0x00);

    outb(SERIAL_PORT + 3, 0x80);

    outb(SERIAL_PORT + 0, 0x0C);

    outb(SERIAL_PORT + 3, SERIAL_LCR);
    
    outb(SERIAL_PORT + 2, SERIAL_FCR);
}

void write_serial( char c){
    wait_for_transmit();
    outb(SERIAL_PORT, c);
}

void serial_write_string(const char* str) {
    if (!str) return;  // sanity check

    while (1) {
        char c = *str;
        if (c == '\0') break; // end of string
        write_serial(c);
        str++;  // move to next character
    }
}

void serial_write_dec(int num){
    if (num == 0){
        write_serial('0');
        return;
    }

    if (num < 0) {
        write_serial('-');
        num = -num;
    }

    char buf[12];
    int i = 0;
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--){
        write_serial(buf[i]);
    }
}


void serial_write_hex32(uint32_t num) {
    serial_write_string("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nibble = (num >> i) & 0xF;
        write_serial(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}


void serial_write_hex64(uint64_t num) {
    serial_write_string("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (num >> i) & 0xF;
        write_serial(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}