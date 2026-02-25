#pragma once
#ifndef MEMHELP_H
#define MEMHELP_H
#include <stddef.h>
#include <stdint.h>

void* memoryset(void* ptr, int value, size_t num);

void* memcopy(void* dest, const void* src, size_t n);

int memcmps(const void* a, const void* b, size_t n);
#endif // MEMHELP_H


