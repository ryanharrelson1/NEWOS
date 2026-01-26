#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
#include <stddef.h>
#include "../libs/memhelp.h"


#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024
#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2
#define PAGE_USER    0x4
#define RECURSIVE_PAGE_DIR_ADDR 0xFFFFF000
#define PAGE_TABLE_VIRT(pde_idx) (0xFFC00000 | ((pde_idx) << 12))
#define RECURSIVE_PAGE_DIR ((uint32_t*)RECURSIVE_PAGE_DIR_ADDR)
#define KERNEL_TEMP_MAP 0x00200000 
#define TEMP_MAP_PAGES 16




extern uint32_t page_dir[];
uint32_t pde_index(uintptr_t virt);
uint32_t pte_index(uintptr_t virt);


uintptr_t virt_to_phys(uintptr_t virt);

void paging_init();
uintptr_t phys_to_virt(uintptr_t phys);
uintptr_t temp_map_allocate();
void temp_map_free(uintptr_t virt);
void map_kernel_page(uintptr_t virt, uintptr_t phys);
void map_user_page(uintptr_t virt, uintptr_t phys);
void unmap_page_core(uintptr_t virt);



#endif // PAGING_H


