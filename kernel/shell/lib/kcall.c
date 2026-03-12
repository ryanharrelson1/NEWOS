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

int open(const char* path, int flags)
{
    int ret;
    asm volatile("int $0x80"
        : "=a"(ret)
        : "a"(4), "b"(path), "c"(flags));
    return ret;
}

int close(int fd)
{
    int ret;
    asm volatile("int $0x80"
        : "=a"(ret)
        : "a"(5), "b"(fd));
    return ret;
}

int seek(int fd, int pos)
{
    int ret;
    asm volatile("int $0x80"
        : "=a"(ret)
        : "a"(6), "b"(fd), "c"(pos));
    return ret;
}

int list(const char* path)
{
    int ret;
    asm volatile("int $0x80"
        : "=a"(ret)
        : "a"(7), "b"(path));
    return ret;
}

