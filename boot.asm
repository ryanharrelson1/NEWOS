[BITS 32]
[extern kernel_main]
[global _start]

;===========================
; Multiboot2 Header
;===========================

section .multiboot2
align 8

mb2_header:
      dd 0xE85250D6          ; magic
    dd 0                  ; architecture
    dd mb2_header_end - mb2_header  ; length
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header))  ; checksum

    ; Memory map request
    dw 6           ; type = 6
    dw 0           ; flags = 0
    dd 24          ; total size of this tag (header + request fields)
    dd 0           ; reserved / alignment padding

    ; End tag
    dw 0
    dw 0
    dd 8

mb2_header_end:

global mb2_info_ptr
global mb2_magic

section .boot

align 4

mb2_magic: resd 1

mb2_info_ptr: resd 1

;===========================
; BSS / Boot stack
;===========================
section .bss
align 16

stack_bottom:
     resb 16384
 global stack_top
stack_top:


;===========================
; Page tables & directory
;===========================

section .boot
global page_dir
align 4096
page_dir: resd 1024

global page_table_low
page_table_low: resd 1024


global page_table_high
page_table_high: resd 1024

global page_table_vga
align 4096
page_table_vga: resd 1024

;===========================
; Entry point
;===========================

section .boot

 _start:

 

    cli

    mov [mb2_magic], eax
    mov [mb2_info_ptr], ebx

    call setup_paging


    mov eax, page_dir
    mov cr3, eax
    

    mov eax, cr4
    and eax, 0xFFFFFFEF
    mov cr4, eax

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    mov esp, stack_top

    jmp 0xC0000000


.hang:
    hlt
    jmp .hang

;===========================
; Paging Setup Subroutine
;===========================

setup_paging:

    mov edi, page_dir
    mov ecx, 1024
    xor eax, eax
    rep stosd

    mov edi, page_table_low
    mov ecx, 1024
    xor esi, esi

.fill_table_low:
    mov eax, esi
    or eax, 0x3
    mov [edi], eax
    add esi,0x1000
    add edi, 4
    loop .fill_table_low

    mov edi, page_table_high
    mov ecx, 1024
    mov esi, 0x00110000

.fill_table_high:

    mov eax, esi
    or eax, 0x3
    mov [edi], eax
    add esi, 0x1000
    add edi, 4
    loop .fill_table_high

    mov eax, page_table_low
    or eax, 0x3
    mov [page_dir], eax

    mov eax, page_table_high
    or eax, 0x3
    mov edi, page_dir
    add edi, 768*4
    mov [edi], eax

     mov eax, 0x000B8000 | 0x3       
    mov edi, page_table_high
    add edi,  184*4                 
    mov [edi], eax

    mov eax, page_dir

    or eax, 0x3
    mov edi, page_dir
    add edi, 1023 * 4
    mov [edi], eax

ret
    



    



