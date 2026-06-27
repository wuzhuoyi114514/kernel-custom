#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE 1u
#define SYS_CLEAR 2u
#define SYS_EXIT  3u
#define SYS_READ  4u
#define SYS_READ_RAW 5u

static inline int sys_read_raw(void) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ_RAW)
        : "memory"
    );
    return ret;
}

static inline int sys_write(int fd, const void *buf, uint32_t len) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static inline int sys_clear(void) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLEAR)
        : "memory"
    );
    return ret;
}

static inline void sys_exit(int status) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(status)
        : "memory"
    );
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static inline int sys_read(int fd, void *buf, uint32_t len) {
    int ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

#endif
