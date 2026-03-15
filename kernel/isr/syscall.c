#include "syscall.h"
#include "../drivers/vga.h"
#include "../drivers/fs/fat.h"
#include "../drivers/fs/vfs.h"
#include "../multitasking/proccess.h"




extern process_t* current_process;

static int fd_alloc(process_t* p) {
    for (int i = 3; i < PROC_MAX_FDS; i++) {
        if (!p->fds[i].used) {
            p->fds[i].used = 1;
            return i;
        }
    }
    return -1;
}

static void fd_free(process_t* p, int fd) {
    if (!p) return;
    if (fd < 0 || fd >= PROC_MAX_FDS) return;
    p->fds[fd].used = 0;
}

static fd_entry_t* fd_get(process_t* p, int fd) {
    if (!p) return 0;

    if (fd < 0 || fd >= PROC_MAX_FDS) return 0;
    //serial_write_string("fd_get: looking for fd entry for fd=");


    if (!p->fds[fd].used){
        serial_write_string("fd_get: fd entry for fd=");
        serial_write_hex32(fd);
        serial_write_string(" is not used\n");
        return 0;
    }

    serial_write_string("fd_get: found fd entry for fd=");
    serial_write_hex32(fd);
    serial_write_string("\n");
    return &p->fds[fd];
}

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
    int fd = (int)r->ebx;
    char* buf = (char*)r->ecx;
    uint32_t len = r->edx;

    if (!buf) return (uint32_t)-1;

    if (fd == 1 || fd == 2) {
        for (uint32_t i = 0; i < len; i++) {
            kprintf_default("%c", buf[i]);
        }
        return len;
    }

    return (uint32_t)-1;
}

static uint32_t sys_exit(regs_t* r) {
    for (;;) {} /* hang for now */
}



static uint32_t sys_open(regs_t* r) {
    process_t* p = current_process;
    if (!p) return (uint32_t)-1;
  serial_write_string("sys_open: current_process=");
serial_write_hex32((uint32_t)p);
serial_write_string("\n");




    const char* path = (const char*)r->ebx;
    if (!path) return (uint32_t)-1;

    int fd = fd_alloc(p);
    if (fd < 0) return (uint32_t)-1;

    serial_write_string("sys_open: fd=");
serial_write_hex32((uint32_t)fd);
serial_write_string("\n");

    serial_write_string("sys_open: used after alloc=");
serial_write_hex32((uint32_t)p->fds[fd].used);
serial_write_string("\n");

    int vr = vfs_open(path, &p->fds[fd].file);

    if (vr != 0) {
        fd_free(p, fd);
        return (uint32_t)-1;
    }

    return (uint32_t)fd;
}

static uint32_t sys_close(regs_t* r) {
    process_t* p = current_process;
    if (!p) return (uint32_t)-1;

    int fd = (int)r->ebx;
    fd_entry_t* e = fd_get(p, fd);
    if (!e) return (uint32_t)-1;

    vfs_close(&e->file);
    fd_free(p, fd);
    return 0;
}

static uint32_t sys_read(regs_t* r) {
    process_t* p = current_process;
    if (!p) return (uint32_t)-1;

    serial_write_string("sys_read: current_process=");
serial_write_hex32((uint32_t)p);
serial_write_string("\n");

    int fd = (int)r->ebx;
    char* buf = (char*)r->ecx;
    uint32_t len = r->edx;

    

    if (!buf) return (uint32_t)-1;


    // stdin
    if (fd == 0) {
        uint32_t i = 0;
        char c;

        asm volatile("sti");

        while (i < len) {
            if (!keyboard_read_char(&c)) {
                asm volatile("hlt");
                continue;
            }

            buf[i++] = c;

            if (c == '\n')
                break;
        }

        return i;
    }


    // file read
    fd_entry_t* e = fd_get(p, fd);
    if (!e) return (uint32_t)-1;
    serial_write_string("Found fd entry for read.\n");


    uint32_t got = 0;
    int vr = vfs_read_file(&e->file, buf, len, &got);
        serial_write_string("vfs_read_file returned vr=");
    serial_write_hex32(vr);
    serial_write_string(" got=");
    serial_write_hex32(got);
    serial_write_string("\n");
    if (vr != 0) return (uint32_t)-1;

    return got;
}

static uint32_t sys_seek(regs_t* r) {
    process_t* p = current_process;
    if (!p) return (uint32_t)-1;

    int fd = (int)r->ebx;
    uint32_t pos = r->ecx;

    fd_entry_t* e = fd_get(p, fd);
    if (!e) return (uint32_t)-1;

    int vr = vfs_seek(&e->file, pos);
    if (vr != 0) return (uint32_t)-1;

    return 0;
}

static uint32_t sys_list(regs_t* r) {
    const char* path = (const char*)r->ebx;
    if (!path) return (uint32_t)-1;

    int vr = vfs_ls(path);
    if (vr != 0) return (uint32_t)-1;

    return 0;
}





/* ---------------------------
   Initialization
   --------------------------- */
void syscall_init(void) {
    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_EXIT,  sys_exit);
    syscall_register(SYS_READ, sys_read);
    syscall_register(SYS_OPEN,  sys_open);
    syscall_register(SYS_CLOSE, sys_close);
    syscall_register(SYS_SEEK,  sys_seek);
    syscall_register(SYS_LIST,  sys_list);
}