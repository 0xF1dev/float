#ifndef FLOAT_STDLIB_H
#define FLOAT_STDLIB_H

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define IPPROTO_TCP 6
#define TCP_CORK 6

#define AT_FDCWD (-100)

#define NULL ((void *)0)

struct sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int sin_addr;
    unsigned char zero[8];
};

struct statx {
    unsigned int stx_mask;
    unsigned int stx_blksize;
    unsigned long long stx_attributes;
    unsigned int stx_nlink;
    unsigned int stx_uid;
    unsigned int stx_gid;
    unsigned short stx_mode;
    unsigned short _pad1[1];
    unsigned long long stx_ino;
    unsigned long long stx_size;
};

unsigned long strlen(const char *str);

int itoa(long num, char *buf, long buf_size);

long atoi(char *str);

long strappend(char *dest, long dest_size, const char *src);

long split(char *str, char delimiter, char **tokens, long max_tokens);

long split_null(char *str, long str_len, char **tokens, long max_tokens);

int contains(const char *str, const char *token);

void strcpy(const char *src, char *dest, long max_len);

int strcmp(const char *s1, const char *s2);

int startswith(const char *str, const char *prefix);

int endswith(const char *str, const char *suffix);

long print(const char *msg);

void print_number(const long num, const int newline);

long syscall3(long num, long arg1, long arg2, long arg3);

long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5);

#endif //FLOAT_STDLIB_H
