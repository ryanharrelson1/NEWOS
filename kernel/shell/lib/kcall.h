#pragma once
#ifndef KCALL_H
#define KCALL_H

int write(int fd, const char* buf, int len);
int read(int fd, char* buf, int len);
void exit(int code);

#endif