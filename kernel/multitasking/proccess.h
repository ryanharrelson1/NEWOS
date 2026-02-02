#ifndef Proccess_H
#define Proccess_H
#include <stdint.h>
#include <stddef.h>
#define PAGE_SIZE 4096
#define USER_STACK_SIZE 0x4000   // 16 KB

#define USER_CODE_VIRT_ADDR_BASE 0x400000 // 4 MB
#define KERNEL_PD_INDEX 768 // == 0xC0000000 >> 22]
#define USER_STACK_VIRT_ADDR_BASE 0x800000 // 8 MB

#define KERNEL_STACK_REGION_START 0xC7000000
#define KERNEL_STACK_REGION_END 0xC8000000
#define KERNEL_STACK_SIZE         (8 * PAGE_SIZE)



typedef struct cpu_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;  // stack pointer (kernel mode)
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t eip;   // Instruction pointer
    uint32_t eflags;
    uint32_t useresp;  // User-mode ESP (SS:ESP needed for iret)
    uint32_t cs;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;
    uint32_t ss;
} cpu_context_t;


typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct process {
    uint32_t pid;
    cpu_context_t context;
    uint32_t page_directory; // Physical address of page directory
    uint32_t kernelstack;    // Kernel stack pointer
    uintptr_t user_stack_top; // Top of user stack
    uintptr_t entry_point;    // Entry point of the process
    task_state_t state;
    struct process* next;     // For linked list of processes
} process_t;

process_t* create_proccess(void* user_code, size_t code_size);
void process_start_user(process_t* proc);
int user_alloc_and_map(uint32_t proc_pd, uintptr_t virt_start, size_t size);
 void copy_user_code(uint32_t proc_pd_phys, uintptr_t dest_virt, const void* src, size_t size);
 void map_user_page_pd(uint32_t target_pd_phys,uintptr_t virt,uintptr_t phys);
uintptr_t alloc_kernel_stack();
void free_kernel_stack(uintptr_t stack_top);
 void cpu_load_cr3(uintptr_t phys_addr);
 uint32_t paging_create_process_directory();



#endif // Proccess_H