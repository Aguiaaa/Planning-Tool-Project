#define _DEFAULT_SOURCE
#include "protocole.h"
#include <endian.h>
#include <stdlib.h>
#include <stdio.h>

ssize_t read_all(int fd, void *buf, size_t count) {
    size_t total = 0;
    ssize_t n;
    while (total < count) {
        n = read(fd, (char*)buf + total, count - total);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

uint16_t read_u16(int fd) {
    uint16_t v;
    if (read_all(fd, &v, 2) < 0) return 0; 
    return be16toh(v);
}

uint32_t read_u32(int fd) {
    uint32_t v;
    if (read_all(fd, &v, 4) < 0) return 0;
    return be32toh(v);
}

uint64_t read_u64(int fd) {
    uint64_t v;
    if (read_all(fd, &v, 8) < 0) return 0;
    return be64toh(v);
}

void write_u16(int fd, uint16_t v) {
    uint16_t be = htobe16(v);
    write(fd, &be, 2);
}

void write_u32(int fd, uint32_t v) {
    uint32_t be = htobe32(v);
    write(fd, &be, 4);
}

void write_u64(int fd, uint64_t v) {
    uint64_t be = htobe64(v);
    write(fd, &be, 8);
}