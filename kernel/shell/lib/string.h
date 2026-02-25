#pragma once
#ifndef STRING_H
#define STRING_H

#define NULL ((void*)0)

unsigned int strlen(const char* s);
int strcmp(const char* a, const char* b);
void puts(const char* s);
void putchar(char c);
void printf(const char* fmt, ...);
int getline(char* buf, int max_len);
char* strtok(char* str, const char* delim);


#endif