#include "../vmm/vmm.h"
#include "../memset.h"

#include "user_mode.h"
#include "../consol/serial.h"
#include "process.h"
#include "../page/paging.h"
#include <stdbool.h>
#include "../page/paging.h"


process_t *current_process = 0;

process_t* process_list = NULL;

static uint32_t next_pid = 1; 
#define PT_LOAD 1
#define ELF_MAGIC 0x464C457F

typedef struct {
    uint32_t magic;      // ELF_MAGIC
    uint8_t  elf[12];    // Unused (EI_NIDENT)
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;      // Entry point
    uint32_t phoff;      // Program header table offset
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;      // Number of program headers
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} Elf32_Phdr;






void scheduler_tick();
bool elf_load(process_t* proc, const uint8_t* elf_data);


static inline uint32_t align_up(uint32_t val) {
    return (val + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}


uintptr_t walk_process_page_tables(uint32_t* pd_phys, uintptr_t virt_addr) {
    uint32_t pd_index = (virt_addr >> 22) & 0x3FF;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    // Temporarily map page directory
      uint32_t* pd_virt = (uint32_t*)vmm_temp_map((uintptr_t)pd_phys, PAGE_PRESENT | PAGE_WRITE);
 

    uint32_t pt_entry_raw = pd_virt[pd_index];
    if (!(pt_entry_raw & PAGE_PRESENT)) {
          vmm_temp_unmap(pd_virt, false); // Unmap temporary mapping
        return 0; // Not present
    }

    uintptr_t pt_phys = pt_entry_raw & ~0xFFF;

      vmm_temp_unmap(pd_virt, false);

    // Temporarily map the page table
    uint32_t* pt_virt = (uint32_t*)vmm_temp_map(pt_phys, PAGE_PRESENT | PAGE_WRITE);



    uint32_t page_entry = pt_virt[pt_index];
    uintptr_t phys = (page_entry & PAGE_PRESENT) ? (page_entry & ~0xFFF) : 0;

    // Unmap temp mappings
    vmm_temp_unmap(pt_virt, false);

    return phys;
}


void copy_to_processs(process_t* proc, void* dst_virt, const void* src_phys_addr, size_t size) {

    write_serial_string("[DEBUG] First byte of src_phys_addr (kernel virt): ");
serial_write_hex32(*(const uint8_t*)src_phys_addr);
write_serial_string(" @ ");
serial_write_hex32((uintptr_t)src_phys_addr);
write_serial_string("\n");

    
    uintptr_t dst = (uintptr_t)dst_virt;
    uintptr_t src_offset = 0;

    const uint8_t* src = (const uint8_t*)src_phys_addr;


     
    

    while (src_offset < size) {
        uintptr_t virt_page = (dst + src_offset) & ~0xFFF;
        uintptr_t page_offset = (dst + src_offset) & 0xFFF;

        size_t copy_len = PAGE_SIZE - page_offset;
        if (copy_len > size - src_offset) {
            copy_len = size - src_offset;
        }

        uintptr_t dst_phys = walk_process_page_tables(proc->page_directory, virt_page);
        if (!dst_phys) {
            panic("copy_to_process: target page not mapped");
        }

        void* dst_virt_temp = vmm_temp_map(dst_phys, PAGE_PRESENT | PAGE_WRITE);
        if (!dst_virt_temp) panic("copy_to_process: failed to map destination");

        // 🔍 DEBUG START
        write_serial_string("[DEBUG] Copying page...\n");

        write_serial_string("  src_phys: ");
        serial_write_hex32(src);
        write_serial_string("  dst_phys: ");
        serial_write_hex32(dst_phys + page_offset);
        write_serial_string("\n");

        write_serial_string("  src_virt_temp: ");
        serial_write_hex32((uintptr_t)src);
        write_serial_string("  dst_virt_temp: ");
        serial_write_hex32((uintptr_t)dst_virt_temp + page_offset);
        write_serial_string("\n");

        write_serial_string("  src_byte: ");
        serial_write_hex32(*(uint8_t*)((uintptr_t)src));
        write_serial_string("\n");
        // 🔍 DEBUG END

        memcpys(
            (void*)((uintptr_t)dst_virt_temp + page_offset),
            (const void*)((uintptr_t)src + src_offset),
            copy_len
        );

        write_serial_string("[DEBUG] First byte of loaded codess: ");
        serial_write_hex32(*(uint8_t*)((uintptr_t)dst_virt_temp + page_offset));
        write_serial_string("\n");

     
        vmm_temp_unmap(dst_virt_temp, false);


        src_offset += copy_len;

         // Just to pause and inspect this iteration
    }
}





process_t* user_space_init(const uint8_t* elf_data, size_t size) {

    process_t* proc = (process_t*)vmm_alloc_kernel(sizeof(process_t));
    if (!proc) {
        panic("Failed to allocate process structure");
    }

     memsets(proc, 0, sizeof(process_t));

      proc->pid = next_pid++;

     uintptr_t kernelstack = vmm_alloc_kernel(PAGE_SIZE * 6);


       proc->kernelstack = kernelstack + PAGE_SIZE * 6; // Set stack top at end of allocated space 

        if (!proc->kernelstack) {
        vmm_free_kernel(proc->user_space_free_list, sizeof(vmm_region_t), true);
        vmm_free_kernel(proc, sizeof(process_t), true);
        panic("Failed to allocate kernel stack for process");
      }

        proc->page_directory = vmm_create_process_page_directory();

        if(!proc->page_directory){

        vmm_free_kernel(proc, sizeof(process_t), true);
        panic("Failed to create process page directory");
        }
        vmm_region_t* user_init = vmm_region_alloc();
        if (!user_init) {
        vmm_free_kernel(proc->page_directory, sizeof(uint32_t) * 1024, true);
        vmm_free_kernel(proc, sizeof(process_t), true);
        panic("Failed to allocate user space region for process");
      }

        user_init->start = USER_VIRT_START;
        user_init->size = USER_VIRT_END - USER_VIRT_START + 1;
        user_init->next = NULL;
        proc->user_space_free_list = user_init; // Initialize user space free list



      if (!elf_load(proc,elf_data)) {
     panic("Failed to load ELF binary");
      }


     void* user_stack = vmm_alloc_user(PAGE_SIZE * 2, proc);
     if (!user_stack) {
        vmm_free_kernel(proc->kernelstack, PAGE_SIZE * 6, true);
        vmm_free_kernel(proc->user_space_free_list, sizeof(vmm_region_t), true);
        vmm_free_kernel(proc, sizeof(process_t), true);
        panic("Failed to allocate user stack for process");
     } 


        proc->user_stack_top = (uintptr_t)user_stack + PAGE_SIZE * 2; // Set stack top at end of allocated space

        proc->state = TASK_READY;

         proc->next = NULL;

   
     write_serial_string("\n[DEBUG] Process initialized with page directory at: ");
        serial_write_hex32((uintptr_t)proc->page_directory);
     write_serial_string("\n");
     write_serial_string("[DEBUG] User stack allocated at: ");
     serial_write_hex32(proc->user_stack_top);
      write_serial_string("\n"); 
     write_serial_string("[DEBUG] user entry allocated at: ");
     serial_write_hex32(proc->entry_point);
        write_serial_string("\n");

        write_serial_string("[DEBUG] Process created successfully\n");
         serial_write_hex32(proc->user_space_free_list->start);
        write_serial_string(" - ");

    proc->context.eip = proc->entry_point; // Set entry point for user mode
    proc->context.cs = 0x1B; // User code segment
    proc->context.ss = 0x23; // User data segment
    proc->context.eflags = 0x202; // Set IF flag for interrupts
    proc->context.useresp = proc->user_stack_top; // Set user stack pointer
    proc->context.ds = 0x23; // User data segment
    proc->context.es = 0x23; // User data segment
    proc->context.fs = 0x23; // User data segment
    proc->context.gs = 0x23; // User data segment
    proc->context.esp = proc->kernelstack; // Kernel stack pointer

    proc->context.eax = 0;
    proc->context.ebx = 0;
    proc->context.ecx = 0;
    proc->context.edx = 0;
    proc->context.esi = 0;
    proc->context.edi = 0;
    proc->context.ebp = 0;


     process_add(proc);
      
    

      return proc;

}



void proccess_load_user_bin(process_t* proc, const uint8_t* bin_start, size_t bin_size) {
   uint32_t aligned_size = align_up(bin_size);

   void* user_virt = vmm_alloc_user(aligned_size, proc);
    if (!user_virt) {
        panic("Failed to allocate user virtual memory for process");
    }


    copy_to_processs(proc, user_virt, bin_start, bin_size);\

    proc->entry_point = (uintptr_t)user_virt; // Set entry point to start of user binary

}

bool elf_load(process_t* proc, const uint8_t* elf_data){
  Elf32_Ehdr* hdr = (Elf32_Ehdr*)elf_data;





  if(hdr->magic != ELF_MAGIC) {
    write_serial_string("[ELF] Invalid ELF magic\n");
    return false;
  }

  Elf32_Phdr* phdr = (Elf32_Phdr*)(elf_data + hdr->phoff);

  for(int i = 0; i < hdr->phnum; i++) {
    Elf32_Phdr* ph = &phdr[i];

    if(ph->type != PT_LOAD) continue;

uintptr_t aligned_vaddr = ph->vaddr & ~0xFFF;
uintptr_t end_vaddr = align_up(ph->vaddr + ph->memsz);
uint32_t total_size = end_vaddr - aligned_vaddr;




     if (aligned_vaddr < USER_VIRT_START || aligned_vaddr + total_size > USER_VIRT_END) {
        write_serial_string("[ELF] Segment vaddr out of user range: ");
        serial_write_hex32(aligned_vaddr);
        panic("ELF segment maps outside user space!");
    }


    void* user_virt = vmm_alloc_user_at(aligned_vaddr, total_size, proc);
    if (!user_virt) {
        write_serial_string("[ELF] Failed to allocate user virtual memory for segment\n");
        return false;
    }   
  
       
                         


      copy_to_processs(proc, user_virt, elf_data + ph->offset, ph->filesz);

         



      if(ph->memsz > ph->filesz) {
        // Zero-fill the rest of the segment
        uint32_t zero_start = (uintptr_t)user_virt + ph->filesz;
        uint32_t zero_size = ph->memsz - ph->filesz;
        memsets((void*)zero_start, 0, zero_size);
      }



  }

   proc->entry_point = hdr->entry; // Set entry point to ELF entry

    write_serial_string("[ELF] Process loaded successfully\n");
    write_serial_string("[ELF] Entry point: ");
    serial_write_hex32(hdr->entry);
    write_serial_string("\n");
    write_serial_string("[ELF] Loading segment:\n");
    write_serial_string("  vaddr: ");
    serial_write_hex32(phdr->vaddr);
     write_serial_string("  memsz: ");
    serial_write_hex32(phdr->memsz);
    write_serial_string("  filesz: ");
        serial_write_hex32(phdr->filesz);
    write_serial_string("\n");  
 
    return true;


}



void scheduler_tick(uintptr_t* stack_frame){

  
    

    current_process->context.eax = stack_frame[7];
    current_process->context.ecx = stack_frame[6];
    current_process->context.edx = stack_frame[5];
    current_process->context.ebx = stack_frame[4];
    current_process->context.ebp = stack_frame[2];
    current_process->context.esi = stack_frame[1];
    current_process->context.edi = stack_frame[0];


    current_process->context.eip = stack_frame[8];
    current_process->context.cs = stack_frame[9];
    current_process->context.eflags = stack_frame[10];
    current_process->context.useresp = stack_frame[11];
    current_process->context.ss = stack_frame[12];

     if (current_process->state == TASK_RUNNING) {
        current_process->state = TASK_READY;
       }

       process_t* next_process = current_process->next;


       while (next_process != current_process && next_process->state != TASK_READY) {
         next_process = next_process->next;
        }
        

        
      

       if (next_process->state != TASK_READY || next_process == current_process) {


    
        return;
        }

     
          current_process = next_process;
        cpu_load_cr3((uintptr_t)current_process->page_directory);

   
       set_kernel_stack(current_process->kernelstack);


      // Restore context into the current stack
        stack_frame[7] = current_process->context.eax;
        stack_frame[6] = current_process->context.ecx;
        stack_frame[5] = current_process->context.edx;
        stack_frame[4] = current_process->context.ebx;
        stack_frame[2] = current_process->context.ebp;
        stack_frame[1] = current_process->context.esi;
        stack_frame[0] = current_process->context.edi;


        stack_frame[8]  = current_process->context.eip;
        stack_frame[9]  = current_process->context.cs;
        stack_frame[10] = current_process->context.eflags;
        stack_frame[11] = current_process->context.useresp;
        stack_frame[12] = current_process->context.ss;

       


}



void scheduler_first_switch() {

   
    cpu_load_cr3((uintptr_t)current_process->page_directory);
    set_kernel_stack(current_process->kernelstack);
  

    uint8_t* probe = (uint8_t*)current_process->entry_point;
    uint8_t val = probe[0]; // safely read the byte

    write_serial_string("[DEBUG] First byte of user code: ");
    serial_write_hex32(val);
    write_serial_string("\n");      
   

    current_process->state = TASK_RUNNING;

 

    
    uintptr_t* stack = (uintptr_t*)(current_process->kernelstack);

        // Build the iret frame on the kernel stack

        *(--stack) = current_process->context.ss;     // EIP
        *(--stack) = current_process->context.useresp;      // CS
        *(--stack) = current_process->context.eflags;  // EFLAGS
        *(--stack) = current_process->context.cs; // ESP
        *(--stack) = current_process->context.eip;      // SS (highest address)

    // Load stack and return to user mode



     
   asm volatile (
    "cli\n"                          // Disable interrupts
    "mov $0x23, %%ax\n"
    "mov %%ax, %%ds\n"
    "mov %%ax, %%es\n"
    "mov %%ax, %%fs\n"
    "mov %%ax, %%gs\n"
    "mov %0, %%esp\n"
    "iret\n"
    : : "r"(stack) : "memory", "ax"
);

    __builtin_unreachable();
}




void process_add(process_t* proc) {
    if (!process_list) {
        // First process: points to itself
        process_list = proc;
        proc->next = proc;
         current_process = proc;
    } else {
        // Insert at end of list (before head for circular)
        process_t* temp = process_list;
        while (temp->next != process_list) {
            temp = temp->next;
        }
        temp->next = proc;
        proc->next = process_list;
    }
}



void process_remove(process_t* proc) {
    if (!process_list) return;

    if (process_list == proc && proc->next == proc) {
        // Only one process
        process_list = NULL;
        return;
    }

    process_t* prev = process_list;
    while (prev->next != proc && prev->next != process_list) {
        prev = prev->next;
    }

    if (prev->next == proc) {
        prev->next = proc->next;
        if (proc == process_list) {
            process_list = proc->next;
        }
    }
}