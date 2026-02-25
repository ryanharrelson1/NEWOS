global _start
extern main 


_start:

   call main

   mov eax, 1
   int 0x80