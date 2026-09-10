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
            return i;
        }
    }

    if (method_not_allowed) {
        return -2; // 405
    }
    return -1; // 404
}

static char *infer_mimetype(const char *filename) {
    if (endswith(filename, ".html") || endswith(filename, ".htm")) {
        return "text/html";
    }
    if (endswith(filename, ".css")) {
        return "text/css";
    }
    if (endswith(filename, ".js")) {
        return "text/javascript";
    }
    if (endswith(filename, ".ttf")) {
        return "font/ttf";
    }
    if (endswith(filename, ".otf")) {
        return "font/otf";
    }
    if (endswith(filename, ".webp")) {
        return "image/webp";
    }
    if (endswith(filename, ".svg")) {
        return "image/svg+xml";
    }
    if (endswith(filename, ".jpg") || endswith(filename, ".jpeg")) {
        return "image/jpeg";
    }
    if (endswith(filename, ".pdf")) {
        return "application/pdf";
    }
    if (endswith(filename, ".json")) {
        return "application/json";
    }
    if (endswith(filename, ".mp4")) {
        return "video/mp4";
    }
    if (endswith(filename, ".mp3")) {
        return "audio/mpeg";
    }
    if (endswith(filename, ".avif")) {
        return "image/avif";
    }
    if (endswith(filename, ".xml")) {
        return "application/xml";
    }
    if (endswith(filename, ".zip")) {
        return "application/zip";
    }
    if (endswith(filename, ".tar")) {
        return "application/x-tar";
    }
    if (endswith(filename, ".tar.gz") || endswith(filename, ".tgz")) {
        return "application/gzip";
    }
    if (endswith(filename, ".tar.bz2") || endswith(filename, ".tbz")) {
        return "application/x-bzip2";
    }
    if (endswith(filename, ".tar.xz") || endswith(filename, ".txz")) {
        return "application/x-xz";
    }
    if (endswith(filename, ".txt")) {
        return "text/plain";
    }
    if (endswith(filename, ".docx")) {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    }
    if (endswith(filename, ".csv")) {
        return "text/csv";
    }
    if (endswith(filename, ".pptx")) {
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    }
    if (endswith(filename, ".xlsx")) {
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    }
    return "application/octet-stream";
}

static char *find_error_page(const struct error *errors, const long errors_count, const int error) {
    for (long i = 0; i < errors_count; i++) {
        if (errors[i].code == error) return errors[i].path;
    }
    return NULL;
}

static void handle_request(long epfd, long ev, int fd, struct config *config, long routes_count, long errors_count,
                           struct sockaddr_in addr) {
    if (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) goto close;

    if (ev & EPOLLIN) {
        char req_buf[4096];

        long read = syscall3(0, fd, (long) req_buf, 4096);
        if (read < 0 && read != -11) {
            print("Could not read request.\n");
            goto close;
        }
        if (read == 0) {
            print("Request done.\n");
            goto close;
        }

        req_buf[read] = '\0';

        if (!headers_done(req_buf)) goto close;


        char *lines[64];
        long count = split(req_buf, '\n', lines, 64);

        int should_close = 0;
        for (int i = 0; i < count; i++) {
            if (contains(lines[i], "Connection: close")) {
                should_close = 1;
            }
        }

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

        long route = match_route(config->routes, routes_count, params[1]);
        char *file_path = NULL;
        if (route == -1) {
            response_status = 404;
            file_path = find_error_page(config->errors, errors_count, response_status);
        } else if (route == -2) {
            response_status = 405;
            file_path = find_error_page(config->errors, errors_count, response_status);
        } else {
            file_path = config->routes[route].path;
        }
        struct statx data;
        data.stx_size = 0;

        if (file_path != NULL) {
            long statx_ret = syscall5(332, AT_FDCWD, (long) file_path, 0, 0x000007ffU, (long) &data);
            if (statx_ret < 0) {
                print("Could not get file info.\n");
            }
        }

        char file_size[32];
        itoa((long) data.stx_size, file_size, 32);

    response:
        print("[");
        print_number(response_status, 0);
        print(" ");
        print(params[1]);
        print("] Received request\n");
        static char res[4096];
        strcpy("HTTP/1.1 ", res, 4096);
        if (response_status == 200) {
            strappend(res, 4096, "200 Ok");
        } else if (response_status == 405) {
            strappend(res, 4096, "405 Method Not Allowed");
        } else if (response_status == 404) {
            strappend(res, 4096, "404 Not Found");
        } else if (response_status == 400) {
            strappend(res, 4096, "400 Bad Request");
        }
        if (file_path != NULL) {
            strappend(res, 4096, "\nContent-Type: ");
            strappend(res, 4096, infer_mimetype(file_path));
            strappend(res, 4096, "\nContent-Length: ");
            strappend(res, 4096, file_size);
        }

        if (response_status != 200) should_close = 1;

        if (should_close) {
            strappend(res, 4096, "\nConnection: close");
        }

        strappend(res, 4096, "\r\n\r\n");

        print("here\n");
        long sent = syscall6(44, fd, (long) &res, (long) strlen(res), MSG_MORE | MSG_NOSIGNAL, 0, 0);
        if (sent < 0) {
            print("Could not send headers.");
            goto close;
        }
        print("no more\n");

        if (file_path != NULL) {
            long file_fd = syscall3(2, (long) file_path, 0, 0);
            if (file_fd < 0) {
                print("Could not open file.");
                goto close;
            }
            long offset = 0;
            long send_ret = syscall5(40, fd, file_fd, (long) &offset, (long) data.stx_size, 0);
            if (send_ret < 0) {
                print("Could not send file.");
                goto close;
            }
        }

        if (!should_close) return;
    }

close:
    long epoll_del = syscall5(233, epfd, EPOLL_CTL_DEL, fd, ev, 0);
    if (epoll_del < 0) {
        print("Could not remove epoll: ");
        print_number(epoll_del, 1);
    }
    long close_ret = syscall3(3, fd, 0, 0);
    if (close_ret < 0) {
        print("Could not close connection.\n");
    }
}


void _start(void) {
    print("Initializing float server...\n");

    // block SIGPIPE
    unsigned long mask = (1ULL << (13 - 1));
    syscall5(14, 0, (long) &mask, 0, 8, 0);

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

    print("Routes:\n");
    for (int i = 0; i < routes_count; i++) {
        if (i != routes_count - 1) {
            print("  ├ ");
        } else {
            print("  └ ");
        }
        print(config.routes[i].route);
        print("\n");
    }

    // open socket
    long server_fd = syscall3(41, AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        print("Could not open socket.");
        goto exit;
    }

    // make non blocking
    int nonblock = 1;
    long nb_ret = syscall3(72, server_fd, 4, O_NONBLOCK);
    if (nb_ret < 0) {
        print("Could not make socket non-blocking.\n");
        goto exit;
    }

    // bind to port (needs the port to be little-endian)
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    // sizeof returns size in bits, so *4 is equivalent to * 8 / 2 (it has to shift the bits by half of the num's size)
    addr.sin_port = (port << sizeof(port) * 4) | (port >> sizeof(port) * 4);
    addr.sin_addr = 0;

    int optval = 1;
    long sockopt_ret = syscall5(54, server_fd, SOL_SOCKET, 2, (long) &optval, sizeof(optval));
    // avoids EADDRINUSE error
    if (sockopt_ret < 0) {
        print("Could not set option.\n");
        goto exit;
    }

    long size = sizeof(addr);

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

    long epfd = syscall3(291, O_CLOEXEC, 0, 0); // EPOLL_CLOEXEC

    struct epoll_event ev, events[128];
    ev.events = EPOLLIN;
    ev.data.fd = (int) server_fd;

    syscall5(233, epfd, EPOLL_CTL_ADD, server_fd, (long) &ev, 0); // 1 = EPOLL_CTL_ADD

    long listen_ret = syscall3(50, server_fd, 512, 0); // listen (backlog of len 512)
    if (listen_ret < 0) {
        print("Could not start listening.\n");
        goto exit;
    }

    print("Server started on port ");
    print_number(port, 0);
    print("!\n");

    while (1) {
        long epoll_ret = syscall5(232, epfd, (long) &events, 128, 10000, 0);
        if (epoll_ret < 0) {
            print("Could not get epoll events.\n");
            continue;
        }

        for (long n = 0; n < epoll_ret; ++n) {
            if (events[n].data.fd == server_fd) {
                while (1) {
                    long size_upd = sizeof(addr);
                    long req_fd = syscall3(43, server_fd, (long) &addr, (long) &size); // accept request
                    if (req_fd < 0) {
                        // all requests have been accepted
                        break;
                    }
                    syscall3(72, req_fd, 4, O_NONBLOCK); // make non blocking

                    int flag = 1;
                    syscall5(54, req_fd, IPPROTO_TCP, TCP_NODELAY, (long) &flag, sizeof(flag));

                    ev.events = EPOLLIN | EPOLLRDHUP;
                    ev.data.fd = (int) req_fd;
                    long ctl_ret = syscall5(233, epfd, EPOLL_CTL_ADD, req_fd, (long) &ev, 0);
                    if (ctl_ret < 0) {
                        print("Could not add epoll.\n");
                        continue;
                    }
                }
            } else {
                handle_request(epfd, events[n].events, events[n].data.fd, &config, routes_count, errors_count,
                               addr);
            }
        }
    }

exit:
    __asm__ volatile ("mov $60, %%rax; xor %%rdi, %%rdi; syscall;" ::: "rax", "rdi");
}
