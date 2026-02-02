#include "elf_loader.h"
#include "../drivers/serial.h"
#include "proccess.h"
#include "../libs/memhelp.h"


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

        // allocate + map
        user_alloc_and_map(pd, base, size);

        // copy file bytes
        copy_user_code(pd,
                       ph[i].vaddr,
                       (uint8_t*)image + ph[i].offset,
                       ph[i].filesz);

        // zero BSS
        uintptr_t bss = ph[i].vaddr + ph[i].filesz;
        size_t bss_sz = ph[i].memsz - ph[i].filesz;
        memoryset((void*)bss, 0, bss_sz);
       
    }

    *entry = eh->entry;
    return true;
}