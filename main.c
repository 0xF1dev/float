#include "stdlib.h"

#define PORT 8080

static int headers_done(const char *buf) {
    const unsigned long len = strlen(buf);
    if (len < 4) return 0;
    for (unsigned long i = 0; i < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') return 1;
    }
    return 0;
}

void _start(void) {
    print("Initializing float server...\n");

    // open socket
    long server_fd = syscall3(41, AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        print("Could not open socket.");
        goto exit;
    }

    // bind to port 8080
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    char msb = PORT >> 8;
    addr.sin_port = (PORT << 8) | msb;
    addr.sin_addr = 0;

    long size = sizeof(addr);

    int optval = 1;
    syscall5(54, server_fd, 1, 2, (long)&optval, sizeof(optval)); // avoids EADDRINUSE error

    long bind_ret = syscall3(49, server_fd, (long) &addr, size); // bind to port 8080
    if (bind_ret < 0) {
        print("Could not bind.\n");
        goto exit;
    }

    long listen_ret = syscall3(50, server_fd, 128, 0); // listen (backlog of len 128)
    if (listen_ret < 0) {
        print("Could not start listening.\n");
        goto exit;
    }

    print("Server started on port ");
    char port_str[5];
    itoa(PORT, port_str, 5);
    print(port_str);
    print("!\n");

    while (1) {
        long req_fd = syscall3(43, server_fd, (long) &addr, (long) &size);
        if (req_fd < 0) {
            print("Could not open request.\n");
            goto close;
        }

        char buf[4096];
        long tot_read = 0;

        while (tot_read < 4095) {
            long read = syscall3(0, req_fd, (long) buf + tot_read, 4095 - tot_read);
            if (read < 0) {
                print("Could not read request.\n");
                goto close;
            } else if (read == 0) {
                print("Request done.\n");
                goto close;
            }

            tot_read += read;
            buf[tot_read] = '\0';

            if (headers_done(buf)) {
                char res[4096] =
                        "HTTP/1.1 200 Ok\r\nContent-Type: text/plain\r\nContent-Length: 12\r\n\r\nHello World!\r\n\r\n";
                long bytes = syscall3(1, req_fd, (long) res, strlen(res));
                if (bytes < 0) {
                    print("Could not send response.");
                }
                break;
            }
        }

    close:
        syscall3(2, req_fd, 0, 0);
    }

exit:
    __asm__ volatile ("mov $60, %%rax; xor %%rdi, %%rdi; syscall;" ::: "rax", "rdi");
}
