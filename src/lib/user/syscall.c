/* syscall.c
 * 系统调用接口
 * 用内联汇编传参并触发中断
 */

#include <syscall.h>

/* 无参数的系统调用 */
#define _syscall0(number) ({    \
    int retval;                 \
    asm volatile (              \
        "int $0x80"             \
        : "=a"(retval)          \
        : "a"(number)           \
        : "memory"              \
    );                          \
    retval;                     \
})

/* 一个参数的系统调用 */
#define _syscall1(NUMBER, ARG1) ({			       \
   int retval;					               \
   asm volatile (					       \
   "int $0x80"						       \
   : "=a" (retval)					       \
   : "a" (NUMBER), "b" (ARG1)				       \
   : "memory"						       \
   );							       \
   retval;						       \
})

/* 两个参数的系统调用 */
#define _syscall2(number, arg1, arg2) ({    \
    int retval;                 \
    asm volatile (              \
        "int $0x80"             \
        : "=a"(retval)          \
        : "a"(number), "b"(arg1), "c"(arg2) \
        : "memory"              \
    );                          \
    retval;                     \
})

/* 三个参数的系统调用 */
#define _syscall3(number, arg1, arg2, arg3) ({    \
    int retval;                 \
    asm volatile (              \
        "int $0x80"             \
        : "=a"(retval)          \
        : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)   \
        : "memory"              \
    );                          \
    retval;                     \
})

/* 返回当前任务pid */
uint32_t getpid(void)
{
    return _syscall0(SYS_GETPID);
}

/* 打印字符串str */
uint32_t write(char *str)
{
    return _syscall1(SYS_WRITE, str);
}

