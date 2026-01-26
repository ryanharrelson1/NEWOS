#include "kernel_heap.h"
#include "../drivers/serial.h"
#include "../libs/memhelp.h"

#define NUM_CACHES 10


static struct kmalloc_cache kmalloc_caches[NUM_CACHES];

static size_t roundup_power_of_two(size_t size) {
    size_t r = 1;
    while (r < size) r <<= 1;
    return r;
}

// initialize caches: 8,16,32,... up to NUM_CACHES
void kmalloc_init() {
    size_t size = 8;
    for (int i = 0; i < NUM_CACHES; i++) {
        kmem_cache_init(&kmalloc_caches[i].cache, size);
        kmalloc_caches[i].size = size;
        size <<= 1;
    }

}

void* kmalloc(size_t size) {
      serial_write_string("kmalloc called with size: ");
    serial_write_hex32(size);
    serial_write_string("\n");

    if (size == 0) return NULL;

    // --- large allocations ---
    if (size > KMALLOC_MAX_SMALL) {
              serial_write_string("kmalloc: large allocation requested\n");
        size_t total_size = size + sizeof(struct kmalloc_header);
        size_t pages = (total_size + SLAB_SIZE - 1) / SLAB_SIZE;

        
        serial_write_string("kmalloc: pages needed = ");
        serial_write_hex32(pages);
        serial_write_string("\n");


        void* virt = heap_alloc_pages(pages);
        if (!virt) return NULL;

          serial_write_string("kmalloc: heap_alloc_pages returned ");
        serial_write_hex32((uintptr_t)virt);
        serial_write_string("\n");




        struct kmalloc_header* header = (struct kmalloc_header*)virt;
        header->size = pages * SLAB_SIZE;
        header->type = 1;      // large
        header->cache = NULL;  // unused for large
         serial_write_string("kmalloc: returning large allocation at ");
        serial_write_hex32((uintptr_t)(header + 1));
        serial_write_string("\n");

        return (void*)(header + 1);
    }

    // --- small allocations (slab) ---
    size_t alloc_size = roundup_power_of_two(size);
       serial_write_string("kmalloc: small allocation, rounded size: ");
    serial_write_hex32(alloc_size);
    serial_write_string("\n");

    for (int i = 0; i < NUM_CACHES; i++) {

         serial_write_string("Checking cache index ");
        serial_write_hex32(i);
        serial_write_string(", cache size = ");
        serial_write_hex32(kmalloc_caches[i].size);
        serial_write_string("\n");

        if (kmalloc_caches[i].size >= alloc_size) {
    
            void* raw = kmem_cache_alloc(&kmalloc_caches[i].cache);
            if (!raw) return NULL;

               serial_write_string("kmem_cache_alloc returned ");
            serial_write_hex32((uintptr_t)raw);
            serial_write_string("\n");


            struct kmalloc_header* header = (struct kmalloc_header*)raw;
            header->size = alloc_size;
            header->type = 0; // slab
            header->cache = &kmalloc_caches[i].cache;
            
            serial_write_string("kmalloc: returning small allocation at ");
            serial_write_hex32((uintptr_t)(header + 1));
            serial_write_string("\n");

            return (void*)(header + 1);
        }
    }
    
    serial_write_string("kmalloc: no suitable cache found\n");
    return NULL;
}



void kfree(void* ptr) {
    if (!ptr) return;

    struct kmalloc_header* header = ((struct kmalloc_header*)ptr) - 1;

    if (header->type == 0) {
        // slab: use cached pointer
        kmem_cache_free(header->cache, header);
    } else if (header->type == 1) {
        // large: unmap & free pages
        size_t pages = (header->size + SLAB_SIZE - 1) / SLAB_SIZE;
        uintptr_t virt = (uintptr_t)header;

        for (size_t i = 0; i < pages; i++) {
            uintptr_t vpage = virt + i * SLAB_SIZE;
            uintptr_t phys = virt_to_phys(vpage);
            if (phys) {
                unmap_page_core(vpage);
                pmm_free_frame(phys);
            }
        }
    }
}