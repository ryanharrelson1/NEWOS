#pragma once
#include <stdint.h>
#include <stdbool.h>

#define ELF_MAGIC 0x464C457F

#define PT_LOAD 1
#define PAGE_SIZE 4096


typedef struct {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) Elf32_Phdr;

bool elf_load(void* image, uint32_t proc_pd_phys, uint32_t* entry_out);