#pragma once
#ifndef SLAB_H
#define SLAB_H
#include <stdint.h>
#include <stddef.h>


#define SLAB_SIZE 4096

#define KERNEL_HEAP_START 0xD2000000 
#define KERNEL_HEAP_END   0xE0000000
#define KERNEL_HEAP_SIZE  (KERNEL_HEAP_END - KERNEL_HEAP_START)




struct slab {
     void *objects;
    void *free_list;
    size_t used;
    size_t capacity;
    size_t object_size;
    struct slab *next;
    uintptr_t phys_addr;
    
};


struct kmem_cache {
   struct slab *slabs;
    size_t object_size;
};



void kmem_cache_init(struct kmem_cache *cache, size_t object_size);
void* kmem_cache_alloc(struct kmem_cache *cache);
void kmem_cache_free(struct kmem_cache *cache, void *obj);
void* heap_alloc_pages(size_t pages);
#endif // SLAB_H