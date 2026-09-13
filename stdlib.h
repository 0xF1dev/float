#ifndef FLOAT_STDLIB_H
#define FLOAT_STDLIB_H

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define IPPROTO_TCP 6
#define TCP_NODELAY 1

#define MSG_MORE 0x8000
#define MSG_NOSIGNAL 0x4000

#define AT_FDCWD (-100)

#define NULL ((void *)0)

#define O_CLOEXEC  02000000
#define O_RDONLY 00

#define PROT_READ 1

#define MAP_SHARED 0x01
#define MAP_FAILED (void *) (-1)

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2

#define EPOLLIN 0x001
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLLRDHUP 0x2000

#define O_NONBLOCK 04000

#define S_IFDIR 0040000
#define S_IFMT 0170000

#define SIGTERM 15
#define PR_SET_PDEATHSIG 1

#define SO_REUSEPORT 15

#define SYS_READ 0
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_FSTAT 5
#define SYS_MMAP 9
#define SYS_RT_SIGPROCMASK 14
#define SYS_WRITEV 20
#define SYS_SENDFILE 40
#define SYS_SOCKET 41
#define SYS_ACCEPT 43
#define SYS_SENDTO 44
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_SETSOCKOPT 54
#define SYS_FORK 57
#define SYS_WAIT4 61
#define SYS_FCNTL 72
#define SYS_PRCTL 157
#define SYS_EPOLLWAIT 232
#define SYS_EPOLLCTL 233
#define SYS_EPOLLCREATE1 291
#define SYS_STATX 332

struct sockaddr {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int sin_addr;
    unsigned char zero[8];
};

// https://git.musl-libc.org/cgit/musl/tree/include/sys/stat.h
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

// https://git.musl-libc.org/cgit/musl/tree/include/sys/epoll.h
typedef union epoll_data {
    void *ptr;
    int fd;
    long u32;
    long long u64;
} epoll_data_t;

struct epoll_event {
    long events;
    epoll_data_t data;
} __attribute__((__packed__));

// https://man.archlinux.org/man/core/man-pages/iovec.3type.en
struct iovec {
    void *iov_base;
    unsigned long iov_len;
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

char *strip_prefix(char *str, const char *prefix);

long print(const char *msg);

void print_number(long num, int newline);

long syscall1(long num, long arg1);

long syscall2(long num, long arg1, long arg2);

long syscall3(long num, long arg1, long arg2, long arg3);

long syscall4(long num, long arg1, long arg2, long arg3, long arg4);

long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5);

long syscall6(long num, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

long syscall0(long num);

long read(int fd, char *buf, long count);
int open(const char *path, int flags);
int close(int fd);
int fstat(int fd, long *statbuf);
char *mmap(void *addr, long length, int prot, int flags, int fd, int offset);
int sigprocmask(int how, unsigned long set, long oldset, int size);
long writev(int fd, struct iovec *iov, int iovcnt);
long sendfile(int out_fd, int in_fd, long *offset, long count);
int socket(int domain, int type, int protocol);
int accept(int sockfd, struct sockaddr *addr, long *addrlen);
long send(int sockfd, char *buf, long size, int flags);
int bind(int sockfd, struct sockaddr *addr, long addrlen);
int listen(int sockfd, int backlog);
int setsockopt(int sockfd, int level, int optname, int *optval, long optlen);
long fork();
long wait4(long pid, int *wstatus, int options, long *rusage);
int fcntl(int fd, int op, long opname);
int prctl(int op, int signal);
int epoll_wait(int epfd, struct epoll_event *events, int n, int timeout);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_create1(int flags);
int statx(int dirfd, char *path, int flags, int mask, struct statx *statxbuf);

#endif //FLOAT_STDLIB_H
