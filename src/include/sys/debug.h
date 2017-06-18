/* debug.h
 * 定义 kassert 宏，供内核使用
 */

#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

#define KDEBUG

void panic(const char *filename, int line, const char *func_name,
                const char *cond);

/* __FILE__表示被编译的文件名，
 * __LINE__表示被编译的行号
 * __func__表示被编译的函数名
 * __VA_ARGS__表示可变参数
 */
#define PANIC(...) panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* 可以在编译的时候用gcc的-D选项来定义此宏，也可以定义在Makefile中 */
#ifdef KDEBUG       /* kernel debug */
    /* 如果条件cond成立，则什么也不做，否则，输出报错信息
     * 符号#让编译器将宏的参数转化为字符串常量
     */
    #define kassert(cond)   \
        if(cond) { } else { \
            PANIC(#cond);   \
        }
#else
    #define kassert(cond)   ((void)0)
#endif  /* KDEBUG */

#endif  /* __KERNEL_DEBUG_H */
