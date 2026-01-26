#include "memorymap.h"
#include "../drivers/serial.h"
#include "../mem/pmm.h"





#define Max_Memory_Regions 32

#define page_size 4096

#define MEM_USABLE 1
#define MEM_RESERVED 0

static inline uint64_t align_up(uint64_t val) {
    return (val + page_size - 1) & ~(page_size - 1);
}

static inline uint64_t align_down(uint64_t val) {
    return val & ~(page_size - 1);
}


static struct mem_region memory_regions[Max_Memory_Regions];

static size_t memory_region_count = 0;


void parse_memory_map(uintptr_t mb_info_addr) {
  

    struct multiboot_tag *tag = (struct multiboot_tag *)(mb_info_addr + 8);


    while (tag->type != MULTIBOOT_TAG_TYPE_END) {

    if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {

        struct multiboot_mmap *mmap_tag =
            (struct multiboot_mmap *)tag;

        uintptr_t entry_addr =
            (uintptr_t)mmap_tag + sizeof(struct multiboot_mmap);

        size_t entry_length =
            mmap_tag->size - sizeof(struct multiboot_mmap);

        for (uintptr_t offset = 0;
             offset < entry_length;
             offset += mmap_tag->entry_size) {

            if (memory_region_count >= Max_Memory_Regions)
                break;

            struct multiboot_mmap_entry *entry =
                (struct multiboot_mmap_entry *)(entry_addr + offset);

            if (entry->type != MULTIBOOT_MEMORY_AVAILABLE)
                continue;

            uint64_t region_start = align_up(entry->addr);
            uint64_t region_end   = align_down(entry->addr + entry->len);

            if (region_end <= region_start)
                continue;

            memory_regions[memory_region_count++] = (struct mem_region){
                .base_addr = region_start,
                .length    = region_end - region_start,
                .type      = MEM_USABLE
            };
        }
    }



    tag = (struct multiboot_tag*)(
        (uintptr_t)tag + ((tag->size + 7) & ~7)
    );
}
    pmm_init(memory_regions, memory_region_count);
}





