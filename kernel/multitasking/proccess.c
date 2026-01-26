#include "proccess.h"
#include "../mem/pmm.h"
#include "../mem/paging.h"
#include "../mem/kernel_heap.h"
#include "../libs/memhelp.h"
#include "../gdt/tss.h"
#include "scheduler.h"

 int next_pid = 1;
 void copy_user_code(uint32_t proc_pd_phys, uintptr_t dest_virt, const void* src, size_t size);
 int user_alloc_and_map(uint32_t proc_pd, uintptr_t virt_start, size_t size);
uint32_t paging_create_process_directory(void);

    
 void cpu_load_cr3(uintptr_t phys_addr) {
    asm volatile("mov %0, %%cr3" :: "r"(phys_addr) : "memory");
}

static uintptr_t next_kernel_stack_va = KERNEL_STACK_REGION_END;


process_t* create_proccess(void* user_code, size_t code_size) {
    serial_write_string("Creating process...\n");

      

    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) return NULL;
  

    serial_write_string("create_proccess: process struct allocated\n");

    proc->pid = next_pid++;
    proc->state = TASK_READY;
    proc->next = NULL;

            
      

          proc->kernelstack = alloc_kernel_stack(proc);
          
             
        // --- Create page directory ---
        proc->page_directory = paging_create_process_directory();
       if (!proc->page_directory) {
        
         serial_write_string("create_proccess: failed to create page directory\n");
        kfree(proc);
        return NULL;
       }
       

       serial_write_string("create_proccess: page directory created\n");

      
  

    

        // --- Load user code into memory ---
           if (user_alloc_and_map(proc->page_directory, USER_CODE_VIRT_ADDR_BASE, code_size) < 0)
            goto fail;

            serial_write_string("create_proccess: user memory allocated and mapped\n");
            serial_write_hex32(code_size);

           

            copy_user_code(proc->page_directory, USER_CODE_VIRT_ADDR_BASE, user_code, code_size);
             

            serial_write_string("create_proccess: user code copied into memory\n");
        

         // --- Set up user stack ---
         user_alloc_and_map(proc->page_directory,0x800000 - USER_STACK_SIZE,USER_STACK_SIZE);

           proc->user_stack_top = USER_STACK_VIRT_ADDR_BASE - 4; // stack grows downwards


         proc->entry_point = USER_CODE_VIRT_ADDR_BASE; // User code starts here

    // --- Initialize CPU context ---
    proc->context.eip = proc->entry_point;
    proc->context.cs = 0x1B; // User code segment
    proc->context.eflags = 0x202; // Interrupts enabled
    proc->context.ss = 0x23; // User data segment
    proc->context.useresp = proc->user_stack_top;
    proc->context.ds = 0x23; // User data segment
    proc->context.es = 0x23; // User data segment
    proc->context.fs = 0x23; // User data segment
    proc->context.gs = 0x23; // User data segment
    


             add_process(proc);
    return proc;

    
       fail:
    if (proc) {
        if (proc->kernelstack)
      
        kfree(proc);
    }
    return NULL;


}


void process_start_user(process_t* proc) {
    set_kernel_stack(proc->kernelstack);
    cpu_load_cr3((uintptr_t)proc->page_directory);

      serial_write_string("process_start_user: switching to user mode\n");

    asm volatile (
        "cli\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        "pushl $0x23\n"              // SS (user data)
        "pushl %[useresp]\n"         // ESP
        "pushl $0x202\n"             // EFLAGS (IF=1)
        "pushl $0x1B\n"              // CS (user code)
        "pushl %[eip]\n"             // EIP
        "iret\n"
        :
        : [useresp]"r"(proc->user_stack_top),
          [eip]"r"(proc->entry_point)
        : "memory"
    );
}






uint32_t paging_create_process_directory() {

    serial_write_hex32  ("paging_create_process_ectorymooommmmmmmmyyyyyyy\n");
      uintptr_t phys = pmm_alloc_frame();
    if (!phys) return 0;

    serial_write_string("paging_create_process_directory: allocated PD frame at ");
    serial_write_hex32(phys);
    serial_write_string("\n");
   uintptr_t temp_pd_virt = temp_map_allocate();
   serial_write_string("paging_create_process_directory: temp PD virt at ");
    serial_write_hex32(temp_pd_virt);
    serial_write_string("\n");
    map_kernel_page(temp_pd_virt, phys);  // RW, present

    uint32_t* pd = (uint32_t*)temp_pd_virt;
    memoryset(pd, 0, PAGE_SIZE);

    serial_write_string("paging_create_process_directory: cleared PD memory\n");
 
    serial_write_string("\n");



    // Copy kernel space (higher half)
    for (int i = KERNEL_PD_INDEX; i < 1024; i++) {
        pd[i] = RECURSIVE_PAGE_DIR[i];
    }


    pd[1023] = phys | PAGE_PRESENT | PAGE_RW;
    

    unmap_page_core(temp_pd_virt);
     serial_write_string("makeing to end \n");
    temp_map_free(temp_pd_virt);
     
  

    return phys;
}







void map_user_page_pd(uint32_t target_pd_phys,uintptr_t virt,uintptr_t phys) {
    // 1) Map target PD into kernel
    uintptr_t temp_pd_virt = temp_map_allocate();
    map_kernel_page(temp_pd_virt, target_pd_phys);

    uint32_t* pd = (uint32_t*)temp_pd_virt;

    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;

    // 2) Ensure page table exists
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        uintptr_t pt_phys = pmm_alloc_frame();
        uintptr_t pt_virt = temp_map_allocate();
        map_kernel_page(pt_virt, pt_phys);
        memoryset((void*)(pt_virt), 0, PAGE_SIZE);
        unmap_page_core(pt_virt);
        temp_map_free(pt_virt);

        pd[pd_index] = pt_phys | PAGE_PRESENT | PAGE_USER | PAGE_RW;
    }

    // 3) Map PT
    uintptr_t pt_phys = pd[pd_index] & ~0xFFF;
    uintptr_t pt_virt = temp_map_allocate();
    map_kernel_page(pt_virt, pt_phys);

    uint32_t* pt = (uint32_t*)(pt_virt);
    pt[pt_index] = phys | PAGE_PRESENT | PAGE_USER | PAGE_RW;

    // 4) Cleanup
    unmap_page_core(pt_virt);
     temp_map_free(pt_virt);
    unmap_page_core(temp_pd_virt);
    temp_map_free(temp_pd_virt);
}


 


int user_alloc_and_map(uint32_t proc_pd, uintptr_t virt_start, size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (size_t i = 0; i < pages; i++) {
        uintptr_t phys = pmm_alloc_frame();
        if (!phys)
            return -1;

        uintptr_t virt = virt_start + i * PAGE_SIZE;
        map_user_page_pd(proc_pd, virt, phys);
    }

    return 0;
}








 void copy_user_code(uint32_t proc_pd_phys, uintptr_t dest_virt, const void* src, size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Map the process page directory temporarily
    uintptr_t temp_pd_virt = temp_map_allocate();
    map_kernel_page(temp_pd_virt, proc_pd_phys);
    uint32_t* pd = (uint32_t*)temp_pd_virt;

    for (size_t i = 0; i < pages; i++) {
        uintptr_t virt = dest_virt + i * PAGE_SIZE;
        uint32_t pd_index = virt >> 22;
        uint32_t pt_index = (virt >> 12) & 0x3FF;

        // Ensure page table exists
        if (!(pd[pd_index] & PAGE_PRESENT))
            for(;;); // PD entry missing

        uintptr_t pt_phys = pd[pd_index] & ~0xFFF;

        // Map PT temporarily
        uintptr_t pt_virt = temp_map_allocate();
        map_kernel_page(pt_virt, pt_phys);
        uint32_t* pt = (uint32_t*)pt_virt;

        if (!(pt[pt_index] & PAGE_PRESENT))
            for(;;); // PT entry missing

        uintptr_t page_phys = pt[pt_index] & ~0xFFF;
        if (!page_phys)
            for(;;); // Invalid physical page

        // Map destination page temporarily
        uintptr_t temp_page = temp_map_allocate();
        map_kernel_page(temp_page, page_phys);

        // Map source page temporarily (user code)
        uintptr_t temp_src = temp_map_allocate();
        map_kernel_page(temp_src, (uintptr_t)src + i * PAGE_SIZE);

        // Determine how much to copy
        size_t copy_size = PAGE_SIZE;
        if ((i + 1) * PAGE_SIZE > size)
            copy_size = size - i * PAGE_SIZE;

        // Copy the data
        memcopy((void*)temp_page, (void*)temp_src, copy_size);

        // Cleanup temporary mappings
        unmap_page_core(temp_page);
        temp_map_free(temp_page);

        unmap_page_core(temp_src);
        temp_map_free(temp_src);

        unmap_page_core(pt_virt);
        temp_map_free(pt_virt);
    }

    // Cleanup PD mapping
    unmap_page_core(temp_pd_virt);
    temp_map_free(temp_pd_virt);
}



uintptr_t alloc_kernel_stack(process_t* proc) {
      serial_write_string("++++++++++++++++++++++++++++++++++++++");
    serial_write_string("Allocating kernel stack for process PID ");
    serial_write_hex32(proc->pid);
    serial_write_string("\n");

    // reserve VA space
    next_kernel_stack_va -= KERNEL_STACK_SIZE;
    serial_write_string("Stack base VA: ");
    serial_write_hex32(next_kernel_stack_va);
    serial_write_string("\n");

    if (next_kernel_stack_va < KERNEL_STACK_REGION_START) {
        serial_write_string("ERROR: Out of kernel stack VA space\n");
        return 0;
    }

    uintptr_t stack_base = next_kernel_stack_va;

    // map physical pages globally
    for (int i = 0; i < KERNEL_STACK_SIZE / PAGE_SIZE; i++) {
        uintptr_t phys = pmm_alloc_frame();
        if (!phys) {
            serial_write_string("ERROR: Out of physical memory for kernel stack\n");
            return 0;
        }

        uintptr_t va = stack_base + i * PAGE_SIZE;
        map_kernel_page(va, phys);

        serial_write_string("Mapped kernel stack page: VA ");
        serial_write_hex32(va);
        serial_write_string(" -> PHYS ");
        serial_write_hex32(phys);
        serial_write_string("\n");
    }

    // optional: ensure small buffer at top
    uintptr_t extra_phys = pmm_alloc_frame();
    if (!extra_phys) {
        serial_write_string("ERROR: Out of physical memory for kernel stack extra top page\n");
    } else {
        uintptr_t extra_va = stack_base + KERNEL_STACK_SIZE;
        map_kernel_page(extra_va, extra_phys);
        serial_write_string("Mapped extra top page: VA ");
        serial_write_hex32(extra_va);
        serial_write_string(" -> PHYS ");
        serial_write_hex32(extra_phys);
        serial_write_string("\n");
    }

    uintptr_t stack_top = stack_base + KERNEL_STACK_SIZE + PAGE_SIZE;
    serial_write_string("Kernel stack TOP returned: ");
    serial_write_hex32(stack_top);
    serial_write_string("\n\n");
     serial_write_string("++++++++++++++++++++++++++++++++++++++");

    // stack grows down → return TOP
    return stack_top;
}