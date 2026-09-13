#include "stdlib.h"

unsigned long strlen(const char *str) {
    const char *p = str;
    while (*p) {
        p++;
    }
    return (unsigned long) (p - str);
}

int itoa(long num, char *buf, const long buf_size) {
    if (buf == NULL || buf_size < 2) return -1;

    long i = 0;
    int is_negative = 0;

    long long n = num;

    if (n < 0) {
        is_negative = 1;
        n = -n;
    } else if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (n > 0) {
        if (i >= buf_size - 1) return -1; // overflow
        buf[i++] = (char) ('0' + n % 10);
        n /= 10;
    }

    if (is_negative) {
        if (i >= buf_size - 1) return -1;
        buf[i++] = '-';
    }

    buf[i] = '\0'; // null-terminate

    long start = 0;
    long end = i - 1;
    while (start < end) {
        char temp = buf[start];
        buf[start] = buf[end];
        buf[end] = temp;
        start++;
        end--;
    }

    return (int) i;
}

long atoi(char *str) {
    char *start = str;
    char *end = str + strlen(str) - 1;

    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }

    long num = 0;
    int count = 1;
    while (*str != '\0') {
        num += (*str - 48) * count;
        count *= 10;
        str++;
    }

    return num;
}

long strappend(char *dest, const long dest_size, const char *src) {
    if (strlen(dest) + strlen(src) + 1 > dest_size) return -1;

    unsigned long i = 0;
    unsigned long dest_len = strlen(dest);

    while (i < strlen(src)) {
        char temp = src[i];
        dest[dest_len + i++] = temp;
    }

    dest[dest_len + i] = '\0';

    return i;
}

long split(char *str, char delimiter, char **tokens, long max_tokens) {
    if (!str || max_tokens == 0) return 0;

    long count = 0;
    int in_token = 0;

    while (*str != '\0' && count < max_tokens) {
        if (*str == delimiter) {
            *str = '\0';
            in_token = 0;
        } else if (!in_token) {
            tokens[count++] = str;
            in_token = 1;
        }
        str++;
    }

    return count;
}

long split_null(char *str, long str_len, char **tokens, long max_tokens) {
    if (!str || max_tokens == 0) return 0;

    char *buf = str;
    char *end = str + str_len;

    long count = 0;
    int in_token = 0;

    while (count < max_tokens && buf < end) {
        if (*buf == '\0') {
            in_token = 0;
        } else if (!in_token) {
            tokens[count++] = buf;
            in_token = 1;
        }
        buf++;
    }

    return count;
}

int contains(const char *str, const char *token) {
    if (*token == '\0') return 1;
    while (*str != '\0') {
        const char *h = str;
        const char *t = token;

        while (*h != '\0' && *t != '\0' && *h == *t) {
            h++;
            t++;
        }

        if (*t == '\0') return 1;

        str++;
    }
    return 0;
}

void strcpy(const char *src, char *dest, long max_len) {
    if (max_len == 0) return;

    long count = 0;

    while (*src != '\0' && count < max_len - 1) {
        dest[count++] = *src;
        src++;
    }
    dest[count] = '\0';
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char) *s1 - (unsigned char) *s2;
}

int startswith(const char *str, const char *prefix) {
    while (*prefix != '\0') {
        if (*str != *prefix) return 0;
        str++;
        prefix++;
    }

    return 1;
}

int endswith(const char *str, const char *suffix) {
    unsigned long suffix_len = strlen(suffix);
    unsigned long str_len = strlen(str);
    if (str_len < suffix_len) return 0;

    const char *offset = str + (str_len - suffix_len);

    while (*suffix != '\0') {
        if (*suffix != *offset) return 0;
        offset++;
        suffix++;
    }

    return 1;
}

inline char *strip_prefix(char *str, const char *prefix) {
    if (startswith(str, prefix)) {
        str += strlen(prefix);
        return str;
    }
    return NULL;
}

inline long print(const char *msg) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (1),
        "D" (1),
        "S" (msg),
        "d" (strlen(msg))
        : "rcx", "r11", "memory"
    );

    return ret;
}

void print_number(const long num, const int newline) {
    char num_buf[11];
    itoa(num, num_buf, 11);
    print(num_buf);
    if (newline) {
        print("\n");
    }
}

inline long syscall1(long num, long arg1) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long syscall2(long num, long arg1, long arg2) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long syscall4(long num, long arg1, long arg2, long arg3, long arg4) {
    long ret;
    register long r10 asm("r10") = arg4;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3), "r" (r10)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5) {
    long ret;
    register long r10 asm("r10") = arg4;
    register long r8 asm("r8") = arg5;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3), "r" (r10), "r" (r8)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long syscall6(long num, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {
    long ret;
    register long r10 asm("r10") = arg4;
    register long r8 asm("r8") = arg5;
    register long r9 asm("r9") = arg6;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3), "r" (r10), "r" (r8), "r" (r9)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long syscall0(long num) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num)
        : "rcx", "r11", "memory"
    );

    return ret;
}

inline long read(const int fd, char *buf, const long count) {
    return syscall3(SYS_READ, fd, (long) buf, count);
}

inline int open(const char *path, const int flags) {
    return (int) syscall2(SYS_OPEN, (long) path, flags);
}

inline int close(const int fd) {
    return (int) syscall1(SYS_CLOSE, fd);
}

inline int fstat(const int fd, long *statbuf) {
    return (int) syscall2(SYS_FSTAT, fd, (long) statbuf);
}

inline char *mmap(void *addr, const long length, const int prot, const int flags, const int fd, int offset) {
    return (char *) syscall6(SYS_MMAP, (long) addr, length, prot, flags, fd, offset);
}

inline int sigprocmask(const int how, unsigned long set, const long oldset, const int size) {
    return (int) syscall4(SYS_RT_SIGPROCMASK, how, (long) &set, oldset, size);
}

inline long writev(const int fd, struct iovec *iov, const int iovcnt) {
    return syscall3(SYS_WRITEV, fd, (long) iov, iovcnt);
}

inline long sendfile(const int out_fd, const int in_fd, long *offset, const long count) {
    return syscall4(SYS_SENDFILE, out_fd, in_fd, (long) offset, count);
}

inline int socket(const int domain, const int type, const int protocol) {
    return (int) syscall3(SYS_SOCKET, domain, type, protocol);
}

inline int accept(const int sockfd, struct sockaddr *addr, long *addrlen) {
    return (int) syscall3(SYS_ACCEPT, sockfd, (long) addr, (long) addrlen);
}

inline long send(const int sockfd, char *buf, const long size, const int flags) {
    return syscall6(SYS_SENDTO, sockfd, (long) buf, size, flags, 0, 0);
}

inline int bind(const int sockfd, struct sockaddr *addr, long addrlen) {
    return (int) syscall3(SYS_BIND, sockfd, (long) addr, addrlen);
}

inline int listen(const int sockfd, const int backlog) {
    return (int) syscall2(SYS_LISTEN, sockfd, backlog);
}

inline int setsockopt(const int sockfd, const int level, const int optname, int *optval, const long optlen) {
    return (int) syscall5(SYS_SETSOCKOPT, sockfd, level, optname, (long) optval, optlen);
}

inline long fork() {
    return syscall0(SYS_FORK);
}

inline long wait4(const long pid, int *wstatus, const int options, long *rusage) {
    return syscall4(SYS_WAIT4, pid, (long) wstatus, options, (long) rusage);
}

inline int fcntl(const int fd, const int op, const long opname) {
    return (int) syscall3(SYS_FCNTL, fd, op, opname);
}

inline int prctl(const int op, const int signal) {
    return (int) syscall2(SYS_PRCTL, op, signal);
}

inline int epoll_wait(const int epfd, struct epoll_event *events, const int n, const int timeout) {
    return (int) syscall4(SYS_EPOLLWAIT, epfd, (long) events, n, timeout);
}

inline int epoll_ctl(const int epfd, const int op, const int fd, struct epoll_event *event) {
    return (int) syscall4(SYS_EPOLLCTL, epfd, op, fd, (long) event);
}

inline int epoll_create1(const int flags) {
    return (int) syscall1(SYS_EPOLLCREATE1, flags);
}

inline int statx(const int dirfd, char *path, const int flags, const int mask, struct statx *statxbuf) {
    return (int) syscall5(SYS_STATX, dirfd, (long) path, flags, mask, (long) statxbuf);
}
