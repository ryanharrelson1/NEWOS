#include "paging.h"


uint32_t pde_index(uintptr_t virt) { return (virt >> 22) & 0x3FF; }
uint32_t pte_index(uintptr_t virt) { return (virt >> 12) & 0x03FF; }

static uint8_t temp_map_used[TEMP_MAP_PAGES] = {0};



static inline void flush_tlb(uintptr_t addr) {
    asm volatile("invlpg (%0)" :: "r"(addr) : "memory");
}

void map_user_page(uintptr_t virt, uintptr_t phys) {
    uint32_t pde_idx = pde_index(virt);
    uint32_t pte_idx = pte_index(virt);
    
   uint32_t pd_entry = RECURSIVE_PAGE_DIR[pde_idx];
   uint32_t flags = PAGE_USER | PAGE_RW | PAGE_PRESENT;

 

    
    if (!(pd_entry & PAGE_PRESENT)) {
        // Allocate a new page table
        uintptr_t new_pt_phys = pmm_alloc_frame();
        RECURSIVE_PAGE_DIR[pde_idx] = new_pt_phys | PAGE_RW | PAGE_PRESENT | PAGE_USER;

        // Clear the new page table via recursive mapping
        uint32_t* new_pt_virt = (uint32_t*)PAGE_TABLE_VIRT(pde_idx);
        memoryset(new_pt_virt, 0, PAGE_SIZE);

    }

    uint32_t* pt = (uint32_t*)PAGE_TABLE_VIRT(pde_idx);
    pt[pte_idx] = phys & ~0xFFF | flags;
    
    flush_tlb(virt);

  
}

void map_kernel_page(uintptr_t virt, uintptr_t phys) {
    uint32_t pde_idx = pde_index(virt);
    uint32_t pte_idx = pte_index(virt);
    
   uint32_t pd_entry = RECURSIVE_PAGE_DIR[pde_idx];

   uint32_t flags = PAGE_RW | PAGE_PRESENT;

    
    if (!(pd_entry & PAGE_PRESENT)) {
        // Allocate a new page table
        uintptr_t new_pt_phys = pmm_alloc_frame();
        RECURSIVE_PAGE_DIR[pde_idx] = new_pt_phys | PAGE_RW | PAGE_PRESENT;

        // Clear the new page table via recursive mapping
        uint32_t* new_pt_virt = (uint32_t*)PAGE_TABLE_VIRT(pde_idx);
        memoryset(new_pt_virt, 0, PAGE_SIZE);

    }

    uint32_t* pt = (uint32_t*)PAGE_TABLE_VIRT(pde_idx);
    pt[pte_idx] = phys & ~0xFFF | flags;
    
    flush_tlb(virt);

  
}





void unmap_page_core(uintptr_t virt) {
    uint32_t pde_idx = pde_index(virt);
    uint32_t pte_idx = pte_index(virt);

    uint32_t* pde = (uint32_t*)(RECURSIVE_PAGE_DIR + (pde_idx * 4));
    if (!(*pde & PAGE_PRESENT)) {
        return; // Page table not present
    }

    uint32_t* pt = (uint32_t*)PAGE_TABLE_VIRT(pde_idx);
    pt[pte_idx] = 0;

    flush_tlb(virt);

}









uintptr_t virt_to_phys(uintptr_t virt) {
    uint32_t pde_idx = pde_index(virt);
    uint32_t pte_idx = pte_index(virt);

       uint32_t* pde = &RECURSIVE_PAGE_DIR[pde_idx];
    if (!(*pde & PAGE_PRESENT)) return 0;

    uint32_t* pt = (uint32_t*)PAGE_TABLE_VIRT(pde_idx);
    if (!(pt[pte_idx] & PAGE_PRESENT)) return 0;

    return (pt[pte_idx] & ~0xFFF) | (virt & 0xFFF);
}

uintptr_t phys_to_virt(uintptr_t phys) {
    uintptr_t pd_index = phys >> 22;
    uintptr_t pt_index = (phys >> 12) & 0x3FF;
    uintptr_t offset   = phys & 0xFFF;

    // Get virtual address of the page table containing 'phys'
       uint32_t* pt = (uint32_t*)PAGE_TABLE_VIRT(pd_index);

    return (pt[pt_index] & ~0xFFF) + offset;
}


void paging_init() {
   page_dir[0] = 0; // Null page
    flush_tlb(0);
}


uintptr_t temp_map_allocate() {
for (int i = 0; i < TEMP_MAP_PAGES; i++) {
    if(!temp_map_used[i]) {
        temp_map_used[i] = 1;
        return KERNEL_TEMP_MAP + (i * PAGE_SIZE);
    }
}
 return 0; // No free temp map slots

}

void temp_map_free(uintptr_t virt) {
    if (virt < KERNEL_TEMP_MAP || virt >= KERNEL_TEMP_MAP + (TEMP_MAP_PAGES * PAGE_SIZE)) {
        return; // Invalid address
    }
    int index = (virt - KERNEL_TEMP_MAP) / PAGE_SIZE;
    temp_map_used[index] = 0;
}






