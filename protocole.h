#ifndef PROTOCOL_IO_H
#define PROTOCOL_IO_H

#include <stdint.h>
#include <unistd.h>

#define LIST      0x4C53 // 'LS'
#define TIMES_EXITCODES    0x5458 // 'TX'
#define STDOUT    0x534f // 'SO'
#define STDERR    0x5345 // 'SE'
#define TERM      0x4b49 // 'TM'

#define REP_OK      0x4F4B // 'OK'
#define REP_ERR     0x4552 // 'ER'
#define CREATE      0x4352 // 'CR'
#define COMBINE     0x4342 // CI

ssize_t read_all(int fd, void *buf, size_t count);
uint16_t read_u16(int fd);
uint32_t read_u32(int fd);
uint64_t read_u64(int fd);

void write_u16(int fd, uint16_t v);
void write_u32(int fd, uint32_t v);
void write_u64(int fd, uint64_t v);

#endif