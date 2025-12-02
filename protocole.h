#ifndef PROTOCOL_IO_H
#define PROTOCOL_IO_H

#include <stdint.h>
#include <unistd.h>


ssize_t read_all(int fd, void *buf, size_t count);
uint16_t read_u16(int fd);
uint32_t read_u32(int fd);
uint64_t read_u64(int fd);

void write_u16(int fd, uint16_t v);
void write_u32(int fd, uint32_t v);
void write_u64(int fd, uint64_t v);

#endif