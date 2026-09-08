#include "stdlib.h"

#define PORT 8080

enum methods {
    GET,
};

struct route {
    enum methods method;
    char route[128];
    char path[256];
};

static int headers_done(const char *buf) {
    const unsigned long len = strlen(buf);
    if (len < 4) return 0;
    for (unsigned long i = 0; i < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') return 1;
    }
    return 0;
}

static long parse_routes(char *config, struct route *routes) {
    char *entries[512];
    const long count = split(config, '\n', entries, 1024);

    for (long i = 0; i < count; i++) {
        char *data[3];
        split(entries[i], ';', data, 3);
        if (strcmp(data[0], "GET") == 0) {
            routes[i].method = GET;
        } else {
            print("Unsupported method: ");
            print(data[0]);
            print("\n");
            return -1;
        }
        strcpy(data[1], routes[i].route, (long) strlen(data[1]) + 1);
        strcpy(data[2], routes[i].path, (long) strlen(data[2]) + 1);
        strappend(routes[i].path, sizeof(routes[i].path), "\0");
    }

    return count;
}

static long match_route(struct route *routes, long routes_count, char *route, enum methods method) {
    int method_not_allowed = 0;

    for (long i = 0; i < routes_count; i++) {
        if (strcmp(routes[i].route, route) == 0) {
            if (routes[i].method == method) {
                return i;
            }
            method_not_allowed = 1;
        }
    }

    if (method_not_allowed) {
        return -2;
    }
    return -1; // 404
}

static char *infer_mimetype(const char *filename) {
    print(filename);
    if (endswith(filename, ".html")) {
        return "text/html";
    }
    if (endswith(filename, ".css")) {
        return "text/css";
    }
    return "application/octet-stream";
}

void _start(void) {
    if (endswith("../styles.css", ".css")) {
        print("what");
    }

    print("Initializing float server...\n");

    // read config
    char *path = "../routes.conf";
    long conf_fd = syscall3(2, (long) path, 0, 0);
    if (conf_fd < 0) {
        print("Could not open routes file.\n");
        goto exit;
    }

    char config[4096];
    long config_read = syscall3(0, conf_fd, (long) config, 4096);
    if (config_read < 0) {
        print("Could not read config.");
        goto exit;
    }

    struct route routes[256];
    long routes_count = parse_routes(config, routes);

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
    long sockopt_ret = syscall5(54, server_fd, SOL_SOCKET, 2, (long) &optval, sizeof(optval));
    // avoids EADDRINUSE error
    if (sockopt_ret < 0) {
        print("Could not set option.\n");
        goto exit;
    }

    long bind_ret = syscall3(49, server_fd, (long) &addr, size); // bind to port 8080
    if (bind_ret < 0) {
        print("Could not bind");
        if (bind_ret == -98) {
            print(", port in use.\n");
        } else {
            print(".\n");
        }
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

        // close cork (prepare response without sending)
        int cork_optval = 1;
        syscall5(54, req_fd, IPPROTO_TCP, TCP_CORK, (long) &cork_optval, sizeof(cork_optval));

        char req_buf[4096];
        long tot_read = 0;

        while (tot_read < 4095) {
            long read = syscall3(0, req_fd, (long) req_buf + tot_read, 4095 - tot_read);
            if (read < 0) {
                print("Could not read request.\n");
                goto close;
            } else if (read == 0) {
                print("Request done.\n");
                goto close;
            }

            tot_read += read;
            req_buf[tot_read] = '\0';

            if (headers_done(req_buf)) {
                char *lines[64];
                split(req_buf, '\n', lines, 64);

                char *params[3];
                split(lines[0], ' ', params, 3);

                int response_status = 200;

                enum methods method = GET;
                if (strcmp(params[0], "GET") == 0) {
                    method = GET;
                } else {
                    response_status = 405;
                }

                long route = match_route(routes, routes_count, params[1], method);
                if (route == -1) { response_status = 404; } else if (route == -2) { response_status = 405; }
                char *file_path = routes[route].path;
                struct statx data;
                data.stx_size = 0;

                if (response_status == 200) {
                    long statx_ret = syscall5(332, AT_FDCWD, (long) file_path, 0, 0x000007ffU, (long) &data);
                    if (statx_ret < 0) {
                        print("Could not get file info.\n");
                    }
                }

                char file_size[32];
                itoa((long) data.stx_size, file_size, 32);

                char res[4096] = "HTTP/1.1 ";
                if (response_status == 200) {
                    strappend(res, 4096, "200 Ok");
                } else if (response_status == 405) {
                    strappend(res, 4096, "405 Method Not Allowed");
                } else if (response_status == 404) {
                    strappend(res, 4096, "404 Not Found");
                }
                strappend(res, 4096, "\nContent-Type: ");
                strappend(res, 4096, infer_mimetype(file_path));
                strappend(res, 4096, "\nContent-Length: ");
                strappend(res, 4096, file_size);
                strappend(res, 4096, "\r\n\r\n");

                print(file_path);

                long bytes = syscall3(1, req_fd, (long) res, (long) strlen(res));
                if (bytes < 0) {
                    print("Could not send headers.");
                }

                if (response_status != 200) {
                    goto close;
                }

                long file_fd = syscall3(2, (long) file_path, 0, 0);
                if (file_fd < 0) {
                    print("Could not open file.");
                    char path_str[5];
                    itoa(file_fd, path_str, 5);
                    print(path_str);
                    goto close;
                }
                long offset = 0;
                long send_ret = syscall5(40, req_fd, file_fd, (long) &offset, (long) data.stx_size, 0);
                if (send_ret < 0) {
                    print("Could not send file.");
                    goto close;
                }

                break;
            }
        }

        // open cork (sends response)
        cork_optval = 0;
        syscall5(54, req_fd, IPPROTO_TCP, TCP_CORK, (long) &cork_optval, sizeof(cork_optval));

    close:
        syscall3(3, req_fd, 0, 0);
    }

exit:
    __asm__ volatile ("mov $60, %%rax; xor %%rdi, %%rdi; syscall;" ::: "rax", "rdi");
}
