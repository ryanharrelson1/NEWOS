#include "kcall.h"

int write(int fd, const char* buf, int len)
{
    int ret;

    asm volatile("int $0x80"
        : "=a"(ret)
        : "a"(0), "b"(fd), "c"(buf), "d"(len));

    return ret;
}

int read(int fd, char* buf, int len)
{
    int ret;

    asm volatile("int $0x80"
        : "=a"(ret)
        : "a"(3), "b"(fd), "c"(buf), "d"(len));

    return ret;
}

void exit(int code)
{
    asm volatile("int $0x80"
        :
        : "a"(1), "b"(code));

    for(;;);
}