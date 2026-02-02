#pragma once
#include <stdint.h>

#define MAX_SYSCALLS 64

/* Syscall numbers */
enum {
    SYS_WRITE = 0,
    SYS_EXIT  = 1,
};

/* Must match your ISR register frame pushed by pusha */
typedef struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} regs_t;

/* Syscall handler prototype */
typedef uint32_t (*syscall_fn_t)(regs_t* r);

/* Public API */
void syscall_init(void);
void syscall_register(uint32_t num, syscall_fn_t fn);
uint32_t syscall_dispatch(regs_t* r);