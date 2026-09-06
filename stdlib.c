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
        buf[i++] = (char)('0' + n % 10);
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

    return (int)i;
}

long print(const char msg[]) {
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

long syscall3(long num, long arg1, long arg2, long arg3) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3)
        : "rcx", "r11", "memory"
    );

    return ret;
}

long syscall5(long num, long arg1, long arg2, long arg3, long arg4, long arg5) {
    long ret;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a" (num), "D" (arg1), "S" (arg2), "d" (arg3), "r" (arg4), "r" (arg5)
        : "rcx", "r11", "memory"
    );

    return ret;
}
