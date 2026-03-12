#pragma once
#ifndef KCALL_H
#define KCALL_H

int write(int fd, const char* buf, int len);
int read(int fd, char* buf, int len);
void exit(int code);
int open(const char* path, int flags);
int close(int fd);
int seek(int fd, int pos);
int list(const char* path);

#endif