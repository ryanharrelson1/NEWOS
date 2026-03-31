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

bool elf_load_from_path(const char* path, uint32_t pd, uint32_t* entry) {
    if (!path || !entry) {
        serial_write_string("exec dbg: bad args\n");
        return false;
    }

    serial_write_string("exec dbg: elf_load_from_path path = ");
    serial_write_string(path);
    serial_write_string("\n");

    vfs_file_t f;
    if (vfs_open(path, &f) != 0) {
        serial_write_string("exec dbg: vfs_open failed\n");
        return false;
    }

    serial_write_string("exec dbg: opened file\n");

    Elf32_Ehdr eh;
    uint32_t out_read = 0;

    serial_write_string("exec dbg: reading ehdr\n");
    if (vfs_read_at(&f, 0, &eh, sizeof(eh), &out_read) != 0 || out_read != sizeof(eh)) {
        serial_write_string("exec dbg: failed to read ELF header\n");
        vfs_close(&f);
        return false;
    }

    serial_write_string("exec dbg: read ehdr ok\n");
    serial_write_string("exec dbg: eh.magic = ");
    serial_write_hex32(*(uint32_t*)&eh);
    serial_write_string("\n");

    serial_write_string("exec dbg: eh.entry = ");
    serial_write_hex32(eh.entry);
    serial_write_string("\n");

    serial_write_string("exec dbg: eh.phoff = ");
    serial_write_hex32(eh.phoff);
    serial_write_string("\n");

    serial_write_string("exec dbg: eh.phnum = ");
    serial_write_hex32(eh.phnum);
    serial_write_string("\n");

    if (*(uint32_t*)&eh != ELF_MAGIC) {
        serial_write_string("exec dbg: bad ELF magic\n");
        vfs_close(&f);
        return false;
    }

    if (eh.phnum == 0) {
        serial_write_string("exec dbg: eh.phnum is zero\n");
        vfs_close(&f);
        return false;
    }

    size_t ph_size = eh.phnum * sizeof(Elf32_Phdr);
    serial_write_string("exec dbg: ph_size = ");
    serial_write_hex32(ph_size);
    serial_write_string("\n");

    Elf32_Phdr* ph = (Elf32_Phdr*)kmalloc(ph_size);
    if (!ph) {
        serial_write_string("exec dbg: failed to alloc ph table\n");
        vfs_close(&f);
        return false;
    }

    serial_write_string("exec dbg: ph ptr = ");
    serial_write_hex32((uint32_t)ph);
    serial_write_string("\n");

    serial_write_string("exec dbg: reading phdr table\n");
    if (vfs_read_at(&f, eh.phoff, ph, ph_size, &out_read) != 0 || out_read != ph_size) {
        serial_write_string("exec dbg: failed to read phdr table\n");
        kfree(ph);
        vfs_close(&f);
        return false;
    }

    serial_write_string("exec dbg: read phdr table ok\n");

    for (int i = 0; i < eh.phnum; i++) {
        serial_write_string("exec dbg: ph index = ");
        serial_write_hex32(i);
        serial_write_string("\n");

        serial_write_string("exec dbg: ph.type = ");
        serial_write_hex32(ph[i].type);
        serial_write_string("\n");

        serial_write_string("exec dbg: ph.offset = ");
        serial_write_hex32(ph[i].offset);
        serial_write_string("\n");

        serial_write_string("exec dbg: ph.vaddr = ");
        serial_write_hex32(ph[i].vaddr);
        serial_write_string("\n");

        serial_write_string("exec dbg: ph.filesz = ");
        serial_write_hex32(ph[i].filesz);
        serial_write_string("\n");

        serial_write_string("exec dbg: ph.memsz = ");
        serial_write_hex32(ph[i].memsz);
        serial_write_string("\n");

        if (ph[i].type != PT_LOAD) {
            serial_write_string("exec dbg: skipping non-PT_LOAD\n");
            continue;
        }

        uintptr_t base = align_down(ph[i].vaddr);
        size_t size = align_up(ph[i].memsz + (ph[i].vaddr - base));

        serial_write_string("exec dbg: segment base = ");
        serial_write_hex32(base);
        serial_write_string("\n");

        serial_write_string("exec dbg: segment alloc size = ");
        serial_write_hex32(size);
        serial_write_string("\n");

        serial_write_string("exec dbg: user_alloc_and_map begin\n");
        if (user_alloc_and_map(pd, base, size) != 0) {
            serial_write_string("exec dbg: user_alloc_and_map failed\n");
            kfree(ph);
            vfs_close(&f);
            return false;
        }
        serial_write_string("exec dbg: user_alloc_and_map ok\n");

        if (ph[i].filesz > 0) {
            serial_write_string("exec dbg: allocating segment_data\n");
            void* segment_data = kmalloc(ph[i].filesz);
            if (!segment_data) {
                serial_write_string("exec dbg: kmalloc segment_data failed\n");
                kfree(ph);
                vfs_close(&f);
                return false;
            }

            serial_write_string("exec dbg: segment_data ptr = ");
            serial_write_hex32((uint32_t)segment_data);
            serial_write_string("\n");

            serial_write_string("exec dbg: reading segment bytes\n");
            if (vfs_read_at(&f, ph[i].offset, segment_data, ph[i].filesz, &out_read) != 0 ||
                out_read != ph[i].filesz) {
                serial_write_string("exec dbg: failed to read segment bytes\n");
                kfree(segment_data);
                kfree(ph);
                vfs_close(&f);
                return false;
            }
            serial_write_string("exec dbg: read segment bytes ok\n");

            serial_write_string("exec dbg: copy_user_code begin\n");
            copy_user_code(pd, ph[i].vaddr, segment_data, ph[i].filesz);
            serial_write_string("exec dbg: copy_user_code done\n");

            serial_write_string("exec dbg: freeing segment_data\n");
            serial_write_hex32((uint32_t)segment_data);
            serial_write_string("\n");

            kfree(segment_data);

            serial_write_string("exec dbg: freed segment_data ok\n");
        }

        uintptr_t bss = ph[i].vaddr + ph[i].filesz;
        size_t bss_sz = ph[i].memsz - ph[i].filesz;

        serial_write_string("exec dbg: bss start = ");
        serial_write_hex32(bss);
        serial_write_string("\n");

        serial_write_string("exec dbg: bss size = ");
        serial_write_hex32(bss_sz);
        serial_write_string("\n");

        if (bss_sz > 0) {
            uintptr_t start = align_down(bss);
            uintptr_t end   = align_up(bss + bss_sz);

            serial_write_string("exec dbg: bss aligned start = ");
            serial_write_hex32(start);
            serial_write_string("\n");

            serial_write_string("exec dbg: bss aligned end = ");
            serial_write_hex32(end);
            serial_write_string("\n");

            for (uintptr_t addr = start; addr < end; addr += PAGE_SIZE) {
                serial_write_string("exec dbg: bss page addr = ");
                serial_write_hex32(addr);
                serial_write_string("\n");

                uintptr_t phys = get_user_phys(pd, addr);
                serial_write_string("exec dbg: bss page phys = ");
                serial_write_hex32(phys);
                serial_write_string("\n");

                if (!phys) {
                    serial_write_string("exec dbg: get_user_phys failed\n");
                    kfree(ph);
                    vfs_close(&f);
                    return false;
                }

                uintptr_t temp = temp_map_allocate();
                serial_write_string("exec dbg: temp map = ");
                serial_write_hex32(temp);
                serial_write_string("\n");

                map_kernel_page(temp, phys);

                uintptr_t page_start = (addr == start) ? (bss & 0xFFF) : 0;
                uintptr_t page_end =
                    (addr + PAGE_SIZE > bss + bss_sz) ? ((bss + bss_sz) & 0xFFF) : PAGE_SIZE;

                if (page_end == 0)
                    page_end = PAGE_SIZE;

                serial_write_string("exec dbg: bss page_start = ");
                serial_write_hex32(page_start);
                serial_write_string("\n");

                serial_write_string("exec dbg: bss page_end = ");
                serial_write_hex32(page_end);
                serial_write_string("\n");

                memoryset((void*)(temp + page_start), 0, page_end - page_start);

                unmap_page_core(temp);
                temp_map_free(temp);

                serial_write_string("exec dbg: bss page zeroed ok\n");
            }
        }
    }

    *entry = eh.entry;

    serial_write_string("exec dbg: final entry = ");
    serial_write_hex32(*entry);
    serial_write_string("\n");

    serial_write_string("exec dbg: freeing ph table\n");
    serial_write_hex32((uint32_t)ph);
    serial_write_string("\n");

    kfree(ph);

    serial_write_string("exec dbg: closing file\n");
    vfs_close(&f);

    serial_write_string("exec dbg: elf_load_from_path success\n");
    return true;
}