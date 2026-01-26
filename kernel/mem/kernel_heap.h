#pragma once
#ifndef KERNEL_HEAP_H
#define KERNEL_HEAP_H
#include <stdint.h>
#include <stddef.h>
#include "slab.h"

// kernel/mem/kmem.h
struct kmalloc_header {
    uint32_t size;  // size of allocation
    uint8_t  type;  // 0 = slab, 1 = large page
    struct kmem_cache* cache;
};


#define KMALLOC_MAX_SMALL 1024



struct kmalloc_cache{
    struct kmem_cache* cache;
    size_t size;
};


void kmalloc_init();
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif // KERNEL_HEAP_H