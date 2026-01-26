#include "slab.h"
#include "../libs/memhelp.h"
#include "../drivers/serial.h"
#include "../mem/pmm.h"
#include "../mem/paging.h"

static uintptr_t heap_next = KERNEL_HEAP_START;

void* heap_alloc_pages(size_t pages) {
    if (heap_next + pages * PAGE_SIZE > KERNEL_HEAP_END) return NULL;
    uintptr_t start = heap_next;

    for (size_t i = 0; i < pages; i++) {
        uintptr_t phys = pmm_alloc_frame();
        map_kernel_page(heap_next, phys); // map it so the CPU can access it
        heap_next += PAGE_SIZE;
    }

    return (void*)start;
}


static void* slab_alloc(struct slab* s) {
    if (!s->free_list) {
        return NULL; // No free objects
    }
    void* obj = s->free_list;
    s->free_list = *(void**)obj;
    s->used++;
    return obj;
}


static void slab_free(struct slab* s, void* obj) {
    *(void**)obj = s->free_list;
    s->free_list = obj;
    s->used--;
}



static struct slab* create_slab(size_t object_size) {

 serial_write_string("Creating new slab for object size: ");
    void* virt = heap_alloc_pages(1);
    if (!virt) return NULL;

    serial_write_string("Allocated virtual memory for slab at ");
    serial_write_hex32((uintptr_t)virt);
    serial_write_string("\n");
  
    uintptr_t phys = virt_to_phys(virt); // get actual physical frame backing virt

    serial_write_string("Physical address of slab: ");
    serial_write_hex32(phys);

    
    memoryset(virt, 0, SLAB_SIZE);
    
       serial_write_string("broken: ");

    struct slab* s = (struct slab*)virt;



    size_t usable = SLAB_SIZE - sizeof(struct slab);
    size_t capacity = usable / object_size;

    if (capacity == 0) {
    serial_write_string("slab_create: object too large\n");
    return NULL;
    }

    s->phys_addr = phys;
    s->object_size = object_size;
    s->capacity = capacity;
    s->used = 0;
    s->next = NULL;

   uintptr_t base = (uintptr_t)virt + sizeof(struct slab);
    base = (base + (object_size - 1)) & ~(object_size - 1);
    uint8_t* obj_base = (uint8_t*)base;
    s->free_list = obj_base;
    s->objects = obj_base;

 
    void* cur = obj_base;
    for (size_t i = 0; i < capacity - 1; i++) {
        *(void**)cur = cur + object_size;
        cur = *(void**)cur;
    }
    *(void**)cur = NULL;


    return s;
}



void kmem_cache_init(struct kmem_cache* cache, size_t object_size) {
    cache->slabs = 0;
    cache->object_size = object_size;
}



void* kmem_cache_alloc(struct kmem_cache* cache) {

 serial_write_string("kmem_cache_alloc called for cache size: ");
    serial_write_hex32(cache->object_size);
    serial_write_string("\n");

    struct slab* s = cache->slabs;

    // search for slab with free objects
    while (s && s->used == s->capacity) {
          serial_write_string("Skipping full slab at ");
        serial_write_hex32((uintptr_t)s);
        serial_write_string("\n");
        s = s->next;
    }

    if (!s) {
        serial_write_string("No free slab found, creating new slab...\n");
        // no slab with free space, create one
        s = create_slab(cache->object_size);
        if (!s)
        return NULL;

         serial_write_string("Created new slab at ");
        serial_write_hex32((uintptr_t)s);
        serial_write_string("\n");

     

        s->next = cache->slabs;
        cache->slabs = s;
    }
    
        serial_write_string("slab_alloc returned ");
 
    serial_write_string("\n");

    return slab_alloc(s);
}


void kmem_cache_free(struct kmem_cache* cache, void* obj) {
    struct slab* s = cache->slabs;
    while (s) {
       uintptr_t obj_start = (uintptr_t)s->objects; 
        uintptr_t obj_end   = obj_start + s->capacity * s->object_size;
        if ((uintptr_t)obj >= obj_start && (uintptr_t)obj < obj_end) {
            slab_free(s, obj);
            return;
        }
        s = s->next;
    }

    // should not happen
    serial_write_string("kmem_cache_free: object not found in any slab!\n");
}

