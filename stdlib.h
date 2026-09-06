#ifndef FLOAT_STDLIB_H
#define FLOAT_STDLIB_H

#define AF_INET 2
#define SOCK_STREAM 1
#define NULL ((void *)0)

struct sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int sin_addr;
    unsigned char zero[8];
};

unsigned long strlen(const char *str);

int itoa(long num, char *buf, long buf_size);

long print(const char msg[]);

long syscall3(long num, long arg1, long arg2, long arg3);

long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5);

#endif //FLOAT_STDLIB_H
