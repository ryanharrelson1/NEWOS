#include "string.h"
#include <stdarg.h>
#include "kcall.h"



unsigned int strlen(const char* s)
{
    unsigned int i = 0;
    while(s[i]) i++;
    return i;
}

int strcmp(const char* a, const char* b)
{
    while(*a && (*a == *b)) {
        a++;
        b++;
    }

    return *(unsigned char*)a - *(unsigned char*)b;
}

// output one character
void putchar(char c) {
    write(1, &c, 1);
}

// output a string
void puts(const char* s) {
    write(1, s, strlen(s));
}

static void itoa(int value, char* buffer) {
    int i = 0;
    int is_negative = 0;

    if(value == 0) {
        buffer[i++] = '0';
        buffer[i] = 0;
        return;
    }

    if(value < 0) {
        is_negative = 1;
        value = -value;
    }

    while(value) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    if(is_negative) buffer[i++] = '-';

    // reverse
    for(int j=0; j<i/2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i-j-1];
        buffer[i-j-1] = tmp;
    }
    buffer[i] = 0;
}

static void uitoa_hex(unsigned int value, char* buffer) {
    const char* hex = "0123456789abcdef";
    int i = 0;

    if(value == 0) {
        buffer[i++] = '0';
        buffer[i] = 0;
        return;
    }

    while(value) {
        buffer[i++] = hex[value & 0xF];
        value >>= 4;
    }

    // reverse
    for(int j=0; j<i/2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i-j-1];
        buffer[i-j-1] = tmp;
    }
    buffer[i] = 0;
}

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char buffer[32];

    for(int i=0; fmt[i]; i++) {
        if(fmt[i] == '%' && fmt[i+1]) {
            i++;
            switch(fmt[i]) {
                case 'd':
                    itoa(va_arg(args, int), buffer);
                    puts(buffer);
                    break;
                case 'x':
                    uitoa_hex(va_arg(args, unsigned int), buffer);
                    puts(buffer);
                    break;
                case 's':
                    puts(va_arg(args, char*));
                    break;
                case '%':
                    putchar('%');
                    break;
                default:
                    putchar('%');
                    putchar(fmt[i]);
            }
        } else {
            putchar(fmt[i]);
        }
    }

    va_end(args);
}
int getline(char* buf, int max_len) {
    int len = 0;
    putchar('>');  // print prompt
    int start_cursor = 1; // track where input starts

    while(len < max_len - 1) {
        char c;
        int n = 0;

        while((n = read(0, &c, 1)) == 0);

        if(n < 0) break;

        if(c == '\b') {
            if(len > 0) {  // only delete user input, not prompt
                len--;
                putchar('\b');
                putchar(' '); // erase char on screen
                putchar('\b');
            }
            continue;
        }

        buf[len++] = c;
        putchar(c);

        if(c == '\n') {
            putchar('\n');
            break;
        }
    }

    buf[len] = 0;
    return len;
}

char* strtok(char* str, const char* delim) {
    static char* next_token_local = NULL; // safe for this process
    if (str)
        next_token_local = str;

    if (!next_token_local)
        return NULL;

    char* start = next_token_local;
    char* p = start;

    while (*p) {
        const char* d = delim;
        while (*d) {
            if (*p == *d) break;
            d++;
        }
        if (*d) break;
        p++;
    }

    if (*p) {
        *p = 0;
        next_token_local = p + 1;
    } else {
        next_token_local = NULL;
    }

    return start;
}


