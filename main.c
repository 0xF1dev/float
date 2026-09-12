#include "stdlib.h"

#define DEFAULT_PORT 8080
#define DEFAULT_CONFIG "routes.conf"

struct file {
    char *ptr;
    unsigned long long size;
};

struct route {
    char *route;
    char *path;
    struct file file;
};

struct error {
    int code;
    char *path;
};

struct dir {
    char *prefix;
    char *path;
};

struct config {
    struct route routes[256];
    int routes_len;
    struct error errors[64];
    int errors_len;
    struct dir directories[32];
    int dir_len;
};

static unsigned short get_port() {
    const long fd = syscall3(2, (long) "/proc/self/environ", O_RDONLY, 0);
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

static char *get_config_path() {
    const long fd = syscall3(2, (long) "/proc/self/environ", O_RDONLY, 0);
    if (fd < 0) {
        print("Could not get environment variables, using config \"routes.conf\".\n");
        return DEFAULT_CONFIG;
    }

    char buf[8192];
    const long read_ret = syscall3(0, fd, (long) buf, 8192);
    if (read_ret < 0) {
        print("Could not read environment variables, using config \"routes.conf\".\n");
        return DEFAULT_CONFIG;
    }

    syscall3(3, fd, 0, 0); // close

    char *vars[128];
    const long count = split_null(buf, read_ret, vars, 128);

    for (int i = 0; i < count; i++) {
        if (startswith(vars[i], "CONFIG=")) {
            char *val[2];
            if (split(vars[i], '=', val, 2) < 2) return DEFAULT_CONFIG;
            return val[1];
        }
    }

    return DEFAULT_CONFIG;
}

static int headers_done(const char *buf) {
    const unsigned long len = strlen(buf);
    if (len < 4) return 0;
    if (endswith(buf, "\r\n\r\n")) return 1;
    return 0;
}

static long long parse_config(char *config_file, struct config *config) {
    char *entries[512];
    const long count = split(config_file, '\n', entries, 512);

    for (long i = 0; i < count; i++) {
        char *data[3];
        split(entries[i], ';', data, 3);
        if (strcmp(data[0], "GET") == 0) {
            config->routes[config->routes_len].route = data[1];
            config->routes[config->routes_len].path = data[2];
            config->routes_len++;
        } else if (strcmp(data[0], "ERROR") == 0) {
            config->errors[config->errors_len].code = atoi(data[1]);
            config->errors[config->errors_len].path = data[2];
            config->errors_len++;
        } else if (strcmp(data[0], "DIR") == 0) {
            struct statx info = {0};
            long dir_ret = syscall5(332, AT_FDCWD, (long) data[2], 0, 0x00000002U, (long) &info);
            if (dir_ret < 0) {
                print("Could not read static directory defined in config.\n");
                return -1;
            }
            if ((info.stx_mode & S_IFMT) == S_IFDIR) {
                config->directories[config->dir_len].prefix = data[1];
                config->directories[config->dir_len].path = data[2];
                config->dir_len++;
            }
        } else {
            print("Unsupported method: ");
            print(data[0]);
            print("\n");
            return -1;
        }
    }

    return 0;
}

static void cache_routes(struct route *routes, long routes_len) {
    for (long i = 0; i < routes_len; i++) {
        long fd = syscall3(2, (long) routes[i].path, O_RDONLY, 0);
        if (fd < 0) {
            print("Could not open file defined in route.\n");
            print(routes[i].path);
            print_number(fd, 1);
            continue;
        }
        long statbuf[18]; // statbuf[6] is file size
        long stat_ret = syscall3(5, fd, (long) &statbuf, 0);
        if (stat_ret < 0) {
            print("Could not get file info.\n");
            continue;
        }
        char *mmap_ret = (char *) syscall6(9, 0, statbuf[6], PROT_READ, MAP_SHARED, fd, 0);
        if (mmap_ret == MAP_FAILED || mmap_ret == NULL) {
            print("Could not allocate file.\n");
            syscall3(1, fd, 0, 0);
            continue;
        }
        routes[i].file.ptr = mmap_ret;
        routes[i].file.size = statbuf[6];
        syscall3(3, fd, 0, 0);
    }
}

static long match_route(const struct config *config, const char *route) {
    for (long i = 0; i < config->routes_len; i++) {
        if (strcmp(config->routes[i].route, route) == 0) {
            return i;
        }
    }

    for (long i = 0; i < config->dir_len; i++) {
        if (startswith(route, config->directories[i].prefix)) {
            return i | (1 << 30); // flip a bit to differentiate types of routes
        }
    }

    return -1; // 404
}

static char *infer_mimetype(const char *filename) {
    if (endswith(filename, ".html") || endswith(filename, ".htm")) {
        return "text/html; charset=utf-8";
    }
    if (endswith(filename, ".css")) {
        return "text/css";
    }
    if (endswith(filename, ".js")) {
        return "text/javascript";
    }
    if (endswith(filename, ".ico")) {
        return "image/vnd.microsoft.icon";
    }
    if (endswith(filename, ".ttf")) {
        return "font/ttf";
    }
    if (endswith(filename, ".woff")) {
        return "font/woff";
    }
    if (endswith(filename, ".woff2")) {
        return "font/woff2";
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

static long generate_headers(char *buf, char *route, const int status, const char *filename, const char *file_size,
                             int should_close) {
    long written = 9;
    strcpy("HTTP/1.1 ", buf, 2048);
    if (status == 200) {
        written += strappend(buf, 2048, "200 Ok");
    } else if (status == 405) {
        written += strappend(buf, 2048, "405 Method Not Allowed");
    } else if (status == 404) {
        written += strappend(buf, 2048, "404 Not Found");
    } else if (status == 400) {
        written += strappend(buf, 2048, "400 Bad Request");
    }
    if (strcmp(filename, "\0") != 0) {
        written += strappend(buf, 2048, "\r\nContent-Type: ");
        written += strappend(buf, 2048, infer_mimetype(filename));
        written += strappend(buf, 2048, "\r\nContent-Length: ");
        written += strappend(buf, 2048, file_size);
    }
    if (status != 200) should_close = 1;

    if (should_close) {
        written += strappend(buf, 2048, "\r\nConnection: close");
    }

    written += strappend(buf, 2048, "\r\n\r\n");

    return written;
}

static char *find_error_page(const struct error *errors, const long errors_count, const int error) {
    for (long i = 0; i < errors_count; i++) {
        if (errors[i].code == error) return errors[i].path;
    }
    return NULL;
}

static char *find_header(char **lines, int lines_size, char *header) {
    for (int i = 0; i < lines_size; i++) {
        if (startswith(lines[i], header)) {
            char *buf[2];
            split(lines[i], ':', buf, 2);
            buf[1] = strip_prefix(buf[1], " ");
            return buf[1];
        }
    }
    return NULL;
}

static void handle_request(long ev, int fd, struct config *config) {
    if (ev & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)) goto close;

    if (ev & (EPOLLIN | EPOLLOUT)) {
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

        if (!headers_done(req_buf)) return;

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

        char *accepts = find_header(lines, 64, "Accept");

        int response_status = 200;
        char file_path[2048] = "\0";

        if (contains(params[1], "../")) {
            response_status = 400;
            goto response;
        }

        if (strcmp(params[0], "GET") == 1) {
            response_status = 405;
            goto response;
        }

        unsigned long long size = 0;
        long route = match_route(config, params[1]);
        long file_fd = 0;
        if (route == -1) {
            response_status = 404;
            if (contains(accepts, "text/html")) {
                strcpy(find_error_page(config->errors, config->errors_len, response_status), file_path, 2048);
                struct statx data;
                const long statx_ret = syscall5(332, AT_FDCWD, (long) file_path, 0, 0x000007ffU, (long) &data);
                if (statx_ret < 0) {
                    print("Could not stat file.\n");
                    goto response;
                }
                file_fd = syscall3(2, (long) file_path, O_RDONLY, 0);
                if (file_fd < 0) {
                    print("Could not open file: ");
                    print_number(file_fd, 1);
                    goto close;
                }
                size = data.stx_size;
            }
        } else if (route == -2) {
            response_status = 405;
            strcpy(find_error_page(config->errors, config->errors_len, response_status), file_path, 2048);
        } else {
            if (route >= 1073741824) {
                // to differentiate normal routes with directory routes
                strcpy(config->directories[route - 1073741824].path, file_path, 1024);
                strappend(file_path, 256, strip_prefix(params[1], config->directories[route - 1073741824].prefix));

                struct statx data;
                data.stx_size = 0;

                if (strcmp(file_path, "\0") != 0) {
                file_info:
                    const long statx_ret = syscall5(332, AT_FDCWD, (long) file_path, 0, 0x000007ffU, (long) &data);
                    if (statx_ret < 0) {
                        print("Could not stat file.\n");
                        print_number(statx_ret, 1);
                        print("File: ");
                        print(file_path);
                        print("\n\n");
                        response_status = 404;
                        strcpy(find_error_page(config->errors, config->errors_len, 404), file_path, 2048);
                        if (strcmp(file_path, "\0") == 0) {
                            goto file_info;
                        }
                        goto response;
                    }
                    file_fd = syscall3(2, (long) file_path, O_RDONLY, 0);
                    if (file_fd < 0) {
                        print("Could not open file: ");
                        print_number(file_fd, 1);
                        goto close;
                    }
                }
                size = data.stx_size;
            } else {
                strcpy(config->routes[route].path, file_path, 2048);
                size = config->routes[route].file.size;
            }
        }

        if (response_status != 200) should_close = 1;

        char file_size[32];
        itoa((long) size, file_size, 32);

    response:
        print("[");
        print_number(response_status, 0);
        print(" ");
        print(params[1]);
        print("] Received request\n");

        if (route >= 1073741824) {
            char res[2048] = {0};
            const long len = generate_headers(res, params[1], response_status, file_path, file_size, should_close);

            long sent = syscall6(44, fd, (long) &res, len, MSG_MORE | MSG_NOSIGNAL, 0, 0);
            if (sent < 0) {
                print("Could not send headers.\n");
                goto close;
            }

            long offset = 0;
            long send_ret = syscall5(40, fd, file_fd, (long) &offset, (long) size, 0);
            if (send_ret < 0) {
                print("Could not send file.\n");
                goto close;
            }
        } else if (route >= 0) {
            char res[2048] = {0};
            const long len = generate_headers(res, params[1], response_status, file_path, file_size, should_close);

            struct iovec iov[2];
            iov[0].iov_base = res;
            iov[0].iov_len = len;
            iov[1].iov_base = config->routes[route].file.ptr;
            iov[1].iov_len = config->routes[route].file.size;

            const long writev_ret = syscall3(20, fd, (long) iov, 2);
            if (writev_ret < 0) {
                print("Could not send response.\n");
                goto close;
            }
            if ((unsigned long long) writev_ret < len + config->routes[route].file.size) {
                return;
            }
        } else {
            char res[2048] = {0};
            const long len = generate_headers(res, params[1], response_status, file_path, file_size, should_close);

            long sent = syscall6(44, fd, (long) &res, len, MSG_MORE | MSG_NOSIGNAL, 0, 0);
            if (sent < 0) {
                print("Could not send headers.\n");
                goto close;
            }

            if (strcmp(file_path, "\0") != 0) {
                long offset = 0;
                long send_ret = syscall5(40, fd, file_fd, (long) &offset, (long) size, 0);
                if (send_ret < 0) {
                    print("Could not send file.\n");
                    goto close;
                }
            }
        }

        if (!should_close) return;
    }

close:
    const long close_ret = syscall3(3, fd, 0, 0);
    if (close_ret < 0) {
        print("Could not close connection: ");
        print_number(close_ret, 1);
    }
}

void _start(void) {
    print("Initializing float server...\n");

    // block SIGPIPE
    unsigned long mask = (1ULL << (13 - 1));
    syscall5(14, 0, (long) &mask, 0, 8, 0);

    unsigned short port = get_port();

    // read config
    char *path = get_config_path();
    long conf_fd = syscall3(2, (long) path, O_RDONLY, 0);
    if (conf_fd < 0) {
        print("Could not open routes file: ");
        print(path);
        print("\n");
        goto exit;
    }

    char config_buf[4096];
    long config_read = syscall3(0, conf_fd, (long) config_buf, 4096);
    if (config_read < 0) {
        print("Could not read config.");
        goto exit;
    }
    config_buf[config_read] = '\0';

    static struct config config;
    long long config_ret = parse_config(config_buf, &config);
    if (config_ret < 0) {
        goto exit;
    }

    cache_routes(config.routes, config.routes_len);

    print("Routes:\n");
    for (int i = 0; i < config.routes_len; i++) {
        if (i != config.routes_len - 1 || config.dir_len != 0) {
            print("  ├ ");
        } else {
            print("  └ ");
        }
        print(config.routes[i].route);
        print("\n");
    }

    for (int i = 0; i < config.dir_len; i++) {
        if (i != config.dir_len - 1 || config.errors_len != 0) {
            print("  ├ ");
        } else {
            print("  └ ");
        }
        print(config.directories[i].prefix);
        print("/*\n");
    }

    for (int i = 0; i < config.errors_len; i++) {
        if (i != config.errors_len - 1) {
            print("  ├ ");
        } else {
            print("  └ ");
        }
        print_number(config.errors[i].code, 1);
    }

    for (int i = 0; i <= 3; i++) {
        long pid = syscall0(57);
        if (pid == 0) {
            syscall3(157, PR_SET_PDEATHSIG, SIGTERM, 0); // monitor parent process
            goto open_server;
        };
    }
    print("Server started on port ");
    print_number(port, 0);
    print("!\n");
monitor:
    syscall5(61, -1, (long) NULL, 0, 0, 0); // wait for a child process to die
    long pid = syscall0(57); // respawn it
    if (pid != 0) {
        print("Detected terminated child, respawned.\n");
        goto monitor;
    }

open_server:
    // open socket
    long server_fd = syscall3(41, AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        print("Could not open socket.");
        goto exit;
    }

    int reuse = 1;
    long reuse_ret = syscall5(54, server_fd, SOL_SOCKET, SO_REUSEPORT, (long) &reuse, sizeof(reuse));
    if (reuse_ret < 0) {
        print("Could not reuse port.\n");
        goto exit;
    }

    // make non blocking
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

    long listen_ret = syscall3(50, server_fd, 8192, 0); // listen (backlog of len 8192)
    if (listen_ret < 0) {
        print("Could not start listening.\n");
        goto exit;
    }

    while (1) {
        long epoll_ret = syscall5(232, epfd, (long) &events, 128, 10000, 0);
        if (epoll_ret < 0) {
            print("Could not get epoll events.\n");
            continue;
        }

        for (long n = 0; n < epoll_ret; n++) {
            if (events[n].data.fd == server_fd) {
                while (1) {
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
                handle_request(events[n].events, events[n].data.fd, &config);
            }
        }
    }

exit:
    __asm__ volatile ("mov $60, %%rax; xor %%rdi, %%rdi; syscall;" ::: "rax", "rdi");
}
