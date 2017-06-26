/* main.c
 *   测试线程执行
 */

#include <stddef.h>
#include <printk.h>
#include <init.h>
#include <thread.h>
#include <interrupt.h>
#include <console.h>
#include <print.h>
#include <ioqueue.h>
#include <keyboard.h>
#include <process.h>
#include <syscall.h>
#include <sys.h>
#include <stdio.h>

void k_thread_a(void *arg);
void k_thread_b(void *arg);
void u_prog_a(void);
void u_prog_b(void);

int main(void)
{
    put_str("start kernel ... \n");
    init_all();     /* 初始化所有模块 */

    //process_execute(u_prog_a, "user_prog_a");
    //process_execute(u_prog_b, "user_prog_b");

    intr_enable();

    //printk(" I am %-12s, my pid: 0x%x\n", "main", sys_getpid());

    thread_start("k_thread_a", 31, k_thread_a, "k_thread_a");
    thread_start("k_thread_b", 31, k_thread_b, "k_thread_b");

    while (1)
        ;

    return 0;
}

/* 在线程中运行的函数
 * 用void*来通用表示参数，被调用的函数知道自己需要什么类型的参数，
 * 自己转换后再用
 */
void k_thread_a(void *arg)
{
    char * para =arg;
    void * addr = sys_malloc(33);

    printk(" k_thread_b(%d), sys_malloc(33), addr: 0x%x\n", 
                sys_getpid(), addr);

    while(1)
        ;
}

void k_thread_b(void *arg)
{
    char * para =arg;
    void * addr = sys_malloc(63);
    
    printk(" k_thread_b(%d), sys_malloc(63), addr: 0x%x\n", 
                sys_getpid(), addr);
    
    while(1)
        ;
}

/* 测试用户进程 */
void u_prog_a(void)
{
    printf(" u_prog_a(%d) \n", getpid());

    while(1)
        ;
}

/* 测试用户进程 */
void u_prog_b(void)
{
    printf(" u_prog_b(%d) \n", getpid());

    while(1)
        ;
}
