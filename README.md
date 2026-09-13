# Float

Float is a lightweight static HTTP server written in freestanding C, without any libraries.

It supports standard routes, custom error pages and dynamic folder-based routes, and its main purpose is **blogs**.

Since it relies on Linux syscalls, other OSes aren't supported.

## Usage

### Quick start

To test out a simple website, go in the `example/` folder. It contains a config, a few web pages and some assets.

Simply run `float` and the server should start. If it errors with a config error, set the environment variable `CONFIG`
with the path to the routes.conf in the example folder.

### Guide

First of all, you need to write a config file, usually `routes.conf`. It follows a simple structure:

```
METHOD;ROUTE;FILE
```

Each route must be separated by a newline.

`METHOD` can be any of

- GET: the standard static page or file;
- ERR: a custom error page;
- DIR: a dynamic route that will point to files in a directory.

`ROUTE` is either the page's route (e.g. `/` or `/download`) or the error code, if `METHOD` is `ERR` (e.g. `404`, it's
the only possible error).

`FILE` can be a file, in the case of methods `GET` and `ERR`, or a directory in the case of the `DIR` method.

By default, Float will look for a `routes.conf` file in the CWD. To specify where the file is, set the CONFIG
environment variable:

```shell
CONFIG=path/to/routes.conf float
```

And to specify a port other than 8080, set it with the PORT environment variable:

```shell
PORT=6767 float
```

Normally, Float uses 4 separate processes ("workers") to distribute the load better. To use less, set the WORKERS
environment variable:

```shell
WORKERS=2 float
```

## Why use Float?

First of all, it's **lightweight**, coming in at just ~18kb and a base RAM usage of **just 52 kilobytes**.

This is thanks to the fact that Float doesn't use the standard library or any other C library. Even more, it doesn't
make any heap allocation, everything is stored on the stack (except the addresses to the cached routes, but those are OS
allocations and just use 4kb each).

Second, it's **fast** (or so I hope, it was according to my tests). This is still thanks to the absence of any
libraries.

## Architecture

This is going to get pretty technical!

### Caching

Float caches all predefined routes (so standard ones and errors) by using an `mmap` syscall (the only allocation in the
entire server, but they don't really count as allocations, since they're stored by the kernel): this allows, when
needed, to read the desired file faster than simple `open` and `read` syscalls, and therefore, since we just need to
read from pointers, we can use the faster `writev` syscall to write both headers and the file in one go.

And this allows for a bit of (undesired, but still cool) hot reloading, since the pointer reads the file when fetched.
Unfortunately, if the file became longer, the rest of the file that doesn't fit in the size obtained at startup will get
truncated.

### Workers

Next up, the master process forks itself _N_ times to better distribute the load of the requests. Each of these opens a
socket (with the `SO_REUSEPORT` option to allow the other child processes to open their own), makes it non-blocking,
binds it to the port, and creates an `epoll` instance to help with concurrent requests.

### Routing

Now, the server gets to request processing. After reading the request, a function gets called to match the requested
route to the configured ones and returns its index. Since only one value can get returned, I had to get creative: to
differ the standard route indexes and directory indexes, a single bit gets flipped to make the index way bigger and then
subtracting it later. If the route wasn't found, the file to send in the response becomes the defined error page, and if
that isn't present (or the client doesn't have `text/html` in the `Accept` header), just the headers will be returned,
without the body.

Then, if the route was a dynamic route, the requested file in the directory gets sent back with a `write` syscall, and
if it was a standard or error page, it gets sent back with the headers via a `writev` syscall.

### Syscalls used

Here is a list of all the syscalls Float uses:
- `open`
- `read`
- `close`
- `fstat`
- `mmap`
- `fork`
- `prctl`
- `wait4`
- `socket`
- `setsockopt`
- `fcntl`
- `bind`
- `epoll_create1`
- `epoll_ctl`
- `listen`
- `epoll_wait`
- `accept`
- `statx`
- `sendto`
- `sendfile`
- `writev`
