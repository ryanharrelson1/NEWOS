#include "keyboard.h"
#include "../io/io.h"   // inb/outb


static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int buffer_head = 0;
static int buffer_tail = 0;

static bool shift_down = false;
static bool ctrl_down = false;
static bool alt_down = false;
static bool capslock_on = false;
static bool extended = false;

static unsigned char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z',
    'x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

static const unsigned char scancode_map_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z',
    'X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};

enum EXT_KEYS {
    ARROW_UP    = 0x48,
    ARROW_DOWN  = 0x50,
    ARROW_LEFT  = 0x4B,
    ARROW_RIGHT = 0x4D,
};


// ---- Buffer helpers ----
static bool buffer_empty() { return buffer_head == buffer_tail; }
static bool buffer_full() { return ((buffer_head+1) % KEYBOARD_BUFFER_SIZE) == buffer_tail; }
static void buffer_push(char c) { if (!buffer_full()) { keyboard_buffer[buffer_head] = c; buffer_head = (buffer_head+1)%KEYBOARD_BUFFER_SIZE; } }
static bool buffer_pop(char* out) { if (buffer_empty()) return false; *out = keyboard_buffer[buffer_tail]; buffer_tail = (buffer_tail+1)%KEYBOARD_BUFFER_SIZE; return true; }

static bool is_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static char apply_caps_shift(char c){
    if(!is_letter(c)) return c;

    if(capslock_on ^ shift_down)
        return (c >= 'a' && c <= 'z') ? c - 32 : c;

    return c;
}

static void handle_arrow(uint8_t arrow) {
    // You can push a code or handle cursor directly
    // For simplicity, push special codes 0x91..0x94
    switch(arrow) {
        case ARROW_UP:    buffer_push(0x91); break;
        case ARROW_DOWN:  buffer_push(0x92); break;
        case ARROW_LEFT:  buffer_push(0x93); break;
        case ARROW_RIGHT: buffer_push(0x94); break;
    }
}


// ---- IRQ Handler ----
void keyboard_handler() {
    uint8_t scancode = inb(0x60);

    if(scancode == 0xE0) {
        extended = true;
        return;
    }

     if (scancode & 0x80)
        return;

     switch (scancode)
    {
        case 0x2A: case 0x36: shift_down = true; return;
        case 0xAA: case 0xB6: shift_down = false; return;

        case 0x1D: ctrl_down = true; return;
        case 0x9D: ctrl_down = false; return;

        case 0x38: alt_down = true; return;
        case 0xB8: alt_down = false; return;

        case 0x3A: capslock_on = !capslock_on; return;
    }

     if(extended) {
        extended = false;

        switch(scancode){
            case 0x48: buffer_push(0x91); break;
            case 0x50: buffer_push(0x92); break;
            case 0x4B: buffer_push(0x93); break;
            case 0x4D: buffer_push(0x94); break;
        }

        return;
     }





     if(scancode < 0x80){
    char c = shift_down ? scancode_map_shift[scancode] : scancode_map[scancode];
    c = apply_caps_shift(c);

    if(!c) return;

    if(c == '\b'){
        buffer_push('\b');
    }
    else if (c == '\n') {
        buffer_push('\n');
    }
    else {
        buffer_push(c);
    }
}
}

// ---- Public API ----
void keyboard_init() {
    outb(0x64, 0xAE); // enable keyboard IRQ
    outb(0x60, 0xF4); // enable scanning
}

bool keyboard_read_char(char* out) { return buffer_pop(out); }
