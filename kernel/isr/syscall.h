#pragma once
#include <stdint.h>

#define MAX_SYSCALLS 64

/* Syscall numbers */
enum {
    SYS_WRITE = 0,
    SYS_EXIT  = 1,
    SYS_READ = 3,
    SYS_OPEN = 4,
    SYS_CLOSE = 5,
    SYS_SEEK = 6,
    SYS_LIST = 7,
    SYS_FORK = 8,
    SYS_EXEC = 9,
};

typedef struct regss {
    uint32_t gs, fs, es, ds;

    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    uint32_t eip, cs, eflags, useresp, ss;
} regs_t;

/* Syscall handler prototype */
typedef uint32_t (*syscall_fn_t)(regs_t* r);

/* Public API */
void syscall_init(void);
void syscall_register(uint32_t num, syscall_fn_t fn);
uint32_t syscall_dispatch(regs_t* r);