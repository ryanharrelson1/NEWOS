#pragma once
#ifndef MEMHELP_H
#define MEMHELP_H
#include <stddef.h>
#include <stdint.h>

void* memclear(void* ptr, int value, size_t num);

void* memcopy(void* dest, const void* src, size_t n);
#endif // MEMHELP_H


