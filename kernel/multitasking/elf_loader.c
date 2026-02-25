#include "elf_loader.h"
#include "../drivers/serial.h"
#include "proccess.h"
#include "../libs/memhelp.h"
#include "../mem/paging.h"
uintptr_t get_user_phys(uint32_t pd, uintptr_t vaddr);

static uintptr_t align_down(uintptr_t v) {
    return v & ~(PAGE_SIZE-1);
}

static size_t align_up(size_t v) {
    return (v + PAGE_SIZE-1) & ~(PAGE_SIZE-1);
}

bool elf_load(void* image, uint32_t pd, uint32_t* entry)
{
    Elf32_Ehdr* eh = image;

    serial_write_string("ELF magic: ");
    serial_write_hex32(*(uint32_t*)eh); // Should print 0x464C457F

    if (*(uint32_t*)eh != 0x464C457F) {
        serial_write_string("Bad ELF\n");
        return false;
    }

    serial_write_string("Program header offset: ");
    serial_write_hex32(eh->phoff);
    serial_write_string(", number of headers: ");
    serial_write_hex32(eh->phnum);

    Elf32_Phdr* ph = (void*)((uint8_t*)image + eh->phoff);

    for (int i = 0; i < eh->phnum; i++) {
        if (ph[i].type != PT_LOAD)
            continue;

        serial_write_string("Mapping PT_LOAD segment ");
        serial_write_hex32(i);
        serial_write_string(": vaddr=");
        serial_write_hex32(ph[i].vaddr);
        serial_write_string(", filesz=");
        serial_write_hex32(ph[i].filesz);
        serial_write_string(", memsz=");
        serial_write_hex32(ph[i].memsz);
        serial_write_string("\n");

        uintptr_t base = align_down(ph[i].vaddr);
        size_t size = align_up(ph[i].memsz + (ph[i].vaddr - base));

        // allocate + map user pages
        user_alloc_and_map(pd, base, size);

        // copy file bytes into mapped pages
        copy_user_code(pd,
                       ph[i].vaddr,
                       (uint8_t*)image + ph[i].offset,
                       ph[i].filesz);

        // zero BSS
        uintptr_t bss     = ph[i].vaddr + ph[i].filesz;
        size_t bss_sz     = ph[i].memsz - ph[i].filesz;

        uintptr_t start = align_down(bss);
        uintptr_t end   = align_up(bss + bss_sz);

        for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
            uintptr_t temp = temp_map_allocate();
            map_kernel_page(temp, get_user_phys(pd, addr));

            uintptr_t page_start = (addr == start) ? (bss & 0xFFF) : 0;
            uintptr_t page_end   = (addr + PAGE_SIZE > bss + bss_sz) ? ((bss + bss_sz) & 0xFFF) : PAGE_SIZE;

            memoryset((void*)(temp + page_start), 0, page_end - page_start);

            // cleanup
            unmap_page_core(temp);
            temp_map_free(temp);
        }
    }

    *entry = eh->entry;
    return true;
}

uintptr_t get_user_phys(uint32_t pd_phys, uintptr_t virt)
{
    uintptr_t temp_pd = temp_map_allocate();
    map_kernel_page(temp_pd, pd_phys);
    uint32_t* pd = (uint32_t*)temp_pd;

    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;

    if (!(pd[pd_index] & PAGE_PRESENT)) {
        unmap_page_core(temp_pd);
        temp_map_free(temp_pd);
        return 0;
    }

    uintptr_t pt_phys = pd[pd_index] & ~0xFFF;
    uintptr_t temp_pt = temp_map_allocate();
    map_kernel_page(temp_pt, pt_phys);

    uint32_t* pt = (uint32_t*)temp_pt;
    if (!(pt[pt_index] & PAGE_PRESENT)) {
        unmap_page_core(temp_pt);
        temp_map_free(temp_pt);
        unmap_page_core(temp_pd);
        temp_map_free(temp_pd);
        return 0;
    }

    uintptr_t phys = pt[pt_index] & ~0xFFF;

    unmap_page_core(temp_pt);
    temp_map_free(temp_pt);
    unmap_page_core(temp_pd);
    temp_map_free(temp_pd);

    return phys;
}