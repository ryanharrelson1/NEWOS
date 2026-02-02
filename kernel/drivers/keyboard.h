#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 128

void keyboard_init(void);
bool keyboard_read_char(char* out);  // pop from buffer, return false if empty

#endif