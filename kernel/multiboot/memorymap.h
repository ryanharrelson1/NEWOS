#pragma once 
#ifndef MEMMAP_H
#define MEMMAP_H


#include <stdint.h>
#include <stddef.h>

#define MULTIBOOT_MEMORY_AVAILABLE          1
#define MULTIBOOT_TAG_TYPE_MMAP             6
#define MULTIBOOT_TAG_TYPE_END              0


struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed));

struct multiboot_tag{
    uint32_t type;
    uint32_t size;

} __attribute__((packed));

struct mem_region {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type; // optionally store the type for debug/logging
};


void parse_memory_map(uintptr_t mb_info_addr);
void debug_print_memory_regions(void);


#endif // MEMAP_H


