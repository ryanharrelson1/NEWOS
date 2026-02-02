#include "vga.h"
#include "../io/io.h"
#include "serial.h"
#include <stdarg.h>

#define TAB_WIDTH 4


static uint16_t* vga_buffer = (uint16_t*) 0xC00B8000;
static uint8_t vga_color;
static size_t cursor_row = 0;
static size_t cursor_col = 0;



static inline uint8_t make_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}


static inline uint16_t make_entry(char c, uint8_t color) {
    return ((uint16_t)color << 8) | c;
}


static void move_cursor() {
    uint16_t pos = cursor_row * VGA_WIDTH + cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color = make_color(fg, bg);
}


void vga_clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = make_entry(' ', vga_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    move_cursor();
}

static void scroll() {
    if(cursor_row >= VGA_HEIGHT) {
        for(size_t y = 1; y < VGA_HEIGHT; y++) {
            for(size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(y-1)*VGA_WIDTH + x] = vga_buffer[y*VGA_WIDTH + x];
            }
        }
        for(size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(VGA_HEIGHT-1)*VGA_WIDTH + x] = make_entry(' ', vga_color);
        cursor_row = VGA_HEIGHT - 1;
    }
}


void vga_putc(char c) {
    switch(c) {
        case '\n':
            cursor_col = 0;
            cursor_row++;
            break;

        case '\r':
            cursor_col = 0;
            break;

        case '\t':
            cursor_col += TAB_WIDTH;
            if(cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;

        case '\b':
            if(cursor_col > 0) {
                cursor_col--;
            } else if(cursor_row > 0) {
                cursor_row--;
                cursor_col = VGA_WIDTH - 1;
            }
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_entry(' ', vga_color);
            break;

        default:
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_entry(c, vga_color);
            cursor_col++;
            if(cursor_col >= VGA_WIDTH) {
                cursor_col = 0;
                cursor_row++;
            }
            break;
    }

    scroll();
    move_cursor();
}


void vga_write_string(const char* str) {
    for(size_t i = 0; str[i]; i++)
        vga_putc(str[i]);
}

static void print_number(uint32_t num, uint32_t base, int is_signed, int width) {
    char buf[32];
    const char digits[] = "0123456789ABCDEF";
    int i = 0;

    if(is_signed && (int32_t)num < 0) {
        vga_putc('-');
        num = -(int32_t)num;
    }

    if(num == 0) {
        buf[i++] = '0';
    } else {
        while(num > 0) {
            buf[i++] = digits[num % base];
            num /= base;
        }
    }

    // pad with '0' if width is larger
    while(i < width) {
        buf[i++] = '0';
    }

    // print in reverse
    while(i--) {
        vga_putc(buf[i]);
    }
}


void kprintf(enum vga_color fg, enum vga_color bg, const char* fmt, ...) {
    uint8_t old_color = vga_color;
    vga_set_color(fg, bg);

    va_list args;
    va_start(args, fmt);

    for(size_t i = 0; fmt[i]; i++) {
        if(fmt[i] != '%') {
            vga_putc(fmt[i]);
            continue;
        }

        i++; // skip '%'

        switch(fmt[i]) {
            case 'c': {
                char c = (char)va_arg(args, int);
                vga_putc(c);
                break;
            }
            case 's': {
                char* s = va_arg(args, char*);
                for(size_t j = 0; s[j]; j++)
                    vga_putc(s[j]);
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                print_number((uint32_t)val, 10, 1, 0); // width=0
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                print_number(val, 10, 0, 0);
                break;
            }
            case 'x':
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                print_number(val, 16, 0, 0);
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(args, void*);
                vga_putc('0');
                vga_putc('x');
                print_number((uint32_t)val, 16, 0, 8); // always 8 digits
                break;
            }
            case '%': {
                vga_putc('%');
                break;
            }
            default:
                vga_putc('%');
                vga_putc(fmt[i]);
                break;
        }
    }

    va_end(args);
    vga_set_color(old_color & 0x0F, old_color >> 4);
}

void kprintf_default(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for(size_t i = 0; fmt[i]; i++) {
        if(fmt[i] != '%') {
            vga_putc(fmt[i]);
            continue;
        }

        i++; // skip '%'

        switch(fmt[i]) {
            case 'c': {
                char c = (char)va_arg(args, int);
                vga_putc(c);
                break;
            }
            case 's': {
                char* s = va_arg(args, char*);
                for(size_t j = 0; s[j]; j++)
                    vga_putc(s[j]);
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                print_number((uint32_t)val, 10, 1, 0);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                print_number(val, 10, 0, 0);
                break;
            }
            case 'x':
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                print_number(val, 16, 0, 0);
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(args, void*);
                vga_putc('0');
                vga_putc('x');
                print_number((uint32_t)val, 16, 0, 8);
                break;
            }
            case '%': {
                vga_putc('%');
                break;
            }
            default:
                vga_putc('%');
                vga_putc(fmt[i]);
                break;
        }
    }

    va_end(args);
}

void vga_enable_cursor(uint8_t start, uint8_t end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | start);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}


void vga_init() {
    
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_enable_cursor(0, 15);
    
}


void terminal_handle_char(uint8_t c)
{
    switch(c)
    {

        case '\b':
    if(cursor_col > 0 || cursor_row > 0) {
        // move cursor left
        if(cursor_col > 0) {
            cursor_col--;
        } else {
            // move to end of previous line
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        }

        // delete character at cursor and shift rest left
        size_t pos = cursor_row * VGA_WIDTH + cursor_col;
        while (pos < VGA_WIDTH * (cursor_row + 1) - 1 && (vga_buffer[pos + 1] & 0xFF) != ' ') {
            vga_buffer[pos] = vga_buffer[pos + 1];
            pos++;
        }
        vga_buffer[pos] = make_entry(' ', vga_color);

        move_cursor();
    }
    break; 

        case 0x93: // LEFT
            if(cursor_col > 0) {
                cursor_col--;
            } else if(cursor_row > 0) {
                cursor_row--;
                cursor_col = VGA_WIDTH - 1;
            }
            move_cursor();
            break;

        case 0x94: // RIGHT
            if(cursor_col < VGA_WIDTH - 1) {
                cursor_col++;
            } else {
                cursor_col = 0;
                cursor_row++;
            }
            scroll();
            move_cursor();
            break;

        case 0x91: // UP
            if(cursor_row > 0)
                cursor_row--;
            move_cursor();
            break;

        case 0x92: // DOWN
            cursor_row++;
            scroll();
            move_cursor();
            break;

        default:
            vga_putc(c);
            break;
    }
}

