#include "stdlib.h"

#define DEFAULT_PORT 8080

struct route {
    char route[128];
    char path[256];
};

struct error {
    int code;
    char *path;
};

struct config {
    struct route routes[256];
    struct error errors[64];
};

static unsigned short get_port() {
    const long fd = syscall3(2, (long) "/proc/self/environ", 0, 0);
    if (fd < 0) {
        print("Could not get environment variables, using port 8080.\n");
        return DEFAULT_PORT;
    }

    char buf[8192];
    const long read_ret = syscall3(0, fd, (long) buf, 8192);
    if (read_ret < 0) {
        print("Could not read environment variables, using port 8080.\n");
        return DEFAULT_PORT;
    }

    syscall3(3, fd, 0, 0); // close

    char *vars[128];
    const long count = split_null(buf, read_ret, vars, 128);

    for (int i = 0; i < count; i++) {
        if (startswith(vars[i], "PORT=")) {
            char *val[2];
            if (split(vars[i], '=', val, 2) < 2) return DEFAULT_PORT;
            const unsigned short port = (unsigned short) atoi(val[1]);
            return port;
        }
    }

    return DEFAULT_PORT;
}

static int headers_done(const char *buf) {
    const unsigned long len = strlen(buf);
    if (len < 4) return 0;
    for (unsigned long i = 0; i < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') return 1;
    }
    return 0;
}

static long long parse_config(char *config_file, struct config *config) {
    char *entries[512];
    const long count = split(config_file, '\n', entries, 1024);

    long routes = 0;
    long errors = 0;

    for (long i = 0; i < count; i++) {
        char *data[3];
        split(entries[i], ';', data, 3);
        if (strcmp(data[0], "GET") == 0) {
            strcpy(data[1], config->routes[i].route, (long) strlen(data[1]) + 1);
            strcpy(data[2], config->routes[i].path, (long) strlen(data[2]) + 1);
            strappend(config->routes[i].path, sizeof(config->routes[i].path), "\0");
            routes++;
        } else if (strcmp(data[0], "ERROR") == 0) {
            config->errors[errors].code = atoi(data[1]);
            config->errors[errors].path = data[2];
            errors++;
        } else {
            print("Unsupported method: ");
            print(data[0]);
            print("\n");
            return -1;
        }
    }

    long long ret = routes;
    ret <<= 32;
    ret += errors;

    return ret;
}

static long match_route(const struct route *routes, const long routes_count, const char *route) {
    int method_not_allowed = 0;

    for (long i = 0; i < routes_count; i++) {
        if (strcmp(routes[i].route, route) == 0) {
            method_not_allowed = 1;
        }
    }

    if (method_not_allowed) {
        return -2; // 405
    }
    return -1; // 404
}

static char *infer_mimetype(const char *filename) {
    if (endswith(filename, ".html")) {
        return "text/html";
    }
    if (endswith(filename, ".css")) {
        return "text/css";
    }
    return "application/octet-stream";
}

static char *find_error_page(const struct error *errors, const long errors_count, const int error) {
    for (long i = 0; i < errors_count; i++) {
        if (errors[i].code == error) return errors[i].path;
    }
    return "";
}

void _start(void) {
    print("Initializing float server...\n");

    unsigned short port = get_port();

    // read config
    char *path = "../routes.conf";
    long conf_fd = syscall3(2, (long) path, 0, 0);
    if (conf_fd < 0) {
        print("Could not open routes file.\n");
        goto exit;
    }

    char config_buf[4096];
    long config_read = syscall3(0, conf_fd, (long) config_buf, 4096);
    if (config_read < 0) {
        print("Could not read config.");
        goto exit;
    }

    struct config config;
    long long config_ret = parse_config(config_buf, &config);
    long routes_count = config_ret >> 32;
    long errors_count = (config_ret << 32) >> 32;

    // open socket
    long server_fd = syscall3(41, AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        print("Could not open socket.");
        goto exit;
    }


    // bind to port (needs the port to be little-endian)
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    // sizeof returns size in bits, so *4 is equivalent to * 8 / 2 (it has to shift the bits by half of the num's size)
    addr.sin_port = (port << sizeof(port) * 4) | (port >> sizeof(port) * 4);
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
    char port_str[6];
    itoa(port, port_str, 6);
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

                if (contains(params[1], "../")) {
                    response_status = 400;
                    goto response;
                }

                if (strcmp(params[0], "GET") == 1) {
                    response_status = 405;
                    goto response;
                }

                long route = match_route(config.routes, routes_count, params[1]);
                char *file_path = {0};
                if (route == -1) {
                    response_status = 404;
                    file_path = find_error_page(config.errors, errors_count, response_status);
                } else if (route == -2) { response_status = 405; } else {
                    file_path = config.routes[route].path;
                }
                struct statx data;
                data.stx_size = 0;

                long statx_ret = syscall5(332, AT_FDCWD, (long) file_path, 0, 0x000007ffU, (long) &data);
                if (statx_ret < 0) {
                    print("Could not get file info.\n");
                }


                char file_size[32];
                itoa((long) data.stx_size, file_size, 32);

            response:
                print("[");
                print_number(response_status, 0);
                print(" ");
                print(params[1]);
                print("] Sending response...\n");
                char res[4096] = "HTTP/1.1 ";
                if (response_status == 200) {
                    strappend(res, 4096, "200 Ok");
                } else if (response_status == 405) {
                    strappend(res, 4096, "405 Method Not Allowed");
                } else if (response_status == 404) {
                    strappend(res, 4096, "404 Not Found");
                } else if (response_status == 400) {
                    strappend(res, 4096, "400 Bad Request");
                }
                if (file_path != 0) {
                    strappend(res, 4096, "\nContent-Type: ");
                    strappend(res, 4096, infer_mimetype(file_path));
                    strappend(res, 4096, "\nContent-Length: ");
                    strappend(res, 4096, file_size);
                }

                strappend(res, 4096, "\nConnection: close");
                strappend(res, 4096, "\r\n\r\n");

                long bytes = syscall3(1, req_fd, (long) res, (long) strlen(res));
                if (bytes < 0) {
                    print("Could not send headers.");
                }

                if (file_path == 0) {
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

    close:
        // open cork (sends response)
        cork_optval = 0;
        syscall5(54, req_fd, IPPROTO_TCP, TCP_CORK, (long) &cork_optval, sizeof(cork_optval));

        syscall3(3, req_fd, 0, 0);
    }

exit:
    __asm__ volatile ("mov $60, %%rax; xor %%rdi, %%rdi; syscall;" ::: "rax", "rdi");
}
