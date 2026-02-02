void main(){
     asm volatile (
        "movl $0x20, %eax\n"  // put a test number in EAX
    );

    while(1);
}