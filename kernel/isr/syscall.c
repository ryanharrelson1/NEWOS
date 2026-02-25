#include "syscall.h"
#include "../drivers/vga.h"

/* ---------------------------
   Syscall handler table
   --------------------------- */
static syscall_fn_t syscall_table[MAX_SYSCALLS] = {0};

/* ---------------------------
   Public registration API
   --------------------------- */
void syscall_register(uint32_t num, syscall_fn_t fn) {
    if (num < MAX_SYSCALLS) {
        syscall_table[num] = fn;
    }
}

/* ---------------------------
   Dispatcher called from ASM
   --------------------------- */
uint32_t syscall_dispatch(regs_t* r) {
    uint32_t num = r->eax;

    if (num >= MAX_SYSCALLS)
        return (uint32_t)-1;

    syscall_fn_t fn = syscall_table[num];
    if (!fn)
        return (uint32_t)-1;

    return fn(r);
}

/* ---------------------------
   Syscall stubs
   --------------------------- */
static uint32_t sys_write(regs_t* r) {
 char* buf = (char*)r->ecx;
 uint32_t len = r->edx;

 for(uint32_t i = 0; i < len; i++) {
    kprintf_default("%c", buf[i]);
 }

 return len;
}

static uint32_t sys_exit(regs_t* r) {
    for (;;) {} /* hang for now */
}

static uint32_t sys_read(regs_t* r) {
    char* buf = (char*)r->ecx;
    uint32_t len = r->edx;
    uint32_t i = 0;
    char c;

    asm volatile("sti");

    while(i < len) {

        if(!keyboard_read_char(&c)) {
            asm volatile("hlt");
            continue;   // <<< CRITICAL
        }

        buf[i++] = c;

        if(c == '\n')
            break;
    }

    return i;
}

/* ---------------------------
   Initialization
   --------------------------- */
void syscall_init(void) {
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_EXIT,  sys_exit);
    syscall_register(SYS_READ, sys_read);
}