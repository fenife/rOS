/* syscall.h
 */

#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include <stdint.h>

/* 系统调用子功能号 */
enum SYSCALL_NR {
    SYS_GETPID = 0,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE,
    SYS_FORK,
    SYS_READ,
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void * buf, uint32_t count);
void * malloc(uint32_t size);
void free(void * ptr);
int16_t fork(void);
int32_t read(int32_t fd, void* buf, uint32_t count);

#endif  /* __LIB_USER_SYSCALL_H */
