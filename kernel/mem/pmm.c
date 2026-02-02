#include "pmm.h"
#include "../drivers/serial.h"
#include "../libs/memhelp.h"

extern uint64_t _kernel_end_phys;

static uint8_t* bitmap = 0;
static size_t bitmap_size = 0;
static size_t total_pages = 0;
static uint64_t mem_start = 0;
static uint64_t mem_end = 0;

uintptr_t bitmap_phys_start = 0;
uintptr_t bitmap_phys_end   = 0;



static inline void set_frame_used(size_t frame_number) {
    bitmap[frame_number / 8] |= (1 << (frame_number % 8));
}

static inline void set_frame_free(size_t frame_number) {
    bitmap[frame_number / 8] &= ~(1 << (frame_number % 8));
}

static inline int is_frame_used(size_t frame_number) {
    return (bitmap[frame_number / 8] & (1 << (frame_number % 8))) != 0;
}



void pmm_init(struct mem_region* regions, size_t region_count) {
    serial_write_string("Initializing Physical Memory Manager...\n");

    uintptr_t kernel_end_phys = (uintptr_t)&_kernel_end_phys;

       mem_start = UINT64_MAX;
         mem_end = 0;

    for(size_t i =0; i < region_count; i++) {

         if(regions[i].type != 1) 
            continue;
            

        if(regions[i].base_addr < mem_start) {
            mem_start = regions[i].base_addr;
        }

         uint64_t end = regions[i].base_addr + regions[i].length;
        if (end > mem_end)
            mem_end = end;


    }

   total_pages = (mem_end - mem_start + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    bitmap_size = (total_pages + 7) / 8;


    bitmap_phys_start = (kernel_end_phys + PMM_FRAME_SIZE -1) & ~(PMM_FRAME_SIZE - 1);
    bitmap_phys_end = bitmap_phys_start + bitmap_size;


    bitmap = (uint8_t*)(bitmap_phys_start + KERNEL_VIRTUAL_BASE);
    memoryset(bitmap, 0xFF, bitmap_size);

    for (size_t i = 0; i < region_count; i++) {
        if (regions[i].type != 1) {
            continue;
        }

        uint64_t region_start = regions[i].base_addr;
        uint64_t region_end   = regions[i].base_addr + regions[i].length;

        for (uint64_t addr = region_start; addr < region_end; addr += PMM_FRAME_SIZE) {
            if (addr < LOW_MEM_END) {
                continue;
            }

            if (addr < kernel_end_phys)
                continue;


            if (addr >= bitmap_phys_start && addr < bitmap_phys_end) {
                continue;
            }
            size_t page = (addr - mem_start) / PMM_FRAME_SIZE;

            set_frame_free(page);

        }
    }
     set_frame_used(0); // Reserve first frame
    serial_write_string("PMM initialized.\n");
    serial_write_string("phys end");
    serial_write_hex32(bitmap_phys_end);

}


uintptr_t pmm_alloc_frame() {
    for (size_t i = 0; i < total_pages; i++) {
        if (!is_frame_used(i)) {
            set_frame_used(i);
            uintptr_t frame_addr = mem_start + (i * PMM_FRAME_SIZE);
            return frame_addr;
        }
    }
    return 0; // No free frames
}


void pmm_free_frame(void* frame) {
    uintptr_t addr = (uintptr_t)frame;
    if (addr < mem_start || addr >= mem_end) {
        return; // Invalid frame address
    }
    size_t frame_number = (addr - mem_start) / PMM_FRAME_SIZE;
    set_frame_free(frame_number);
}



