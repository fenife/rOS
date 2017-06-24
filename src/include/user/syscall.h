/* syscall.h
 */

#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include <stdint.h>

typedef enum SYSCALL_NR {
    SYS_GETPID = 0,
}SYSCALL_NR;

uint32_t getpid(void);

#endif  /* __LIB_USER_SYSCALL_H */
