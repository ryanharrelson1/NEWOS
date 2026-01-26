[BITS 32]



_start: 
   mov eax, 0x20
    int 0x80        ; return to kernel ONCE
    jmp $