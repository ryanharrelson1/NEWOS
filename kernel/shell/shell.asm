global _starts
extern main 


_starts:

   call main

   mov eax, 1
   int 0x80