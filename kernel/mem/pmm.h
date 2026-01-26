#pragma once

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include "../multiboot/memorymap.h"


#define PMM_FRAME_SIZE 4096
#define LOW_MEM_END 0x100000      // 1 MiB
#define KERNEL_VIRTUAL_BASE 0xC0000000


void pmm_init(struct mem_region* regions, size_t region_count);

uintptr_t pmm_alloc_frame();
void pmm_free_frame(void* frame);


#endif // PMM_H