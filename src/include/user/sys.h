/* sys.h
 */

#ifndef __USERPROG_SYS_H
#define __USERPROG_SYS_H

#include <stdint.h>

void syscall_init(void);

uint32_t sys_getpid(void);
uint32_t sys_write(char *str);

#endif  /* __USERPROG_SYS_H */
