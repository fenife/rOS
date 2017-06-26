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
    put_str("kernel start ... \n");
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
    void * addr1;
    void * addr2;
    void * addr3;
    void * addr4;
    void * addr5;
    void * addr6;
    void * addr7;
    int max = 1000;
    
    printk(" k_thread_a(%d), test start ... \n", sys_getpid());

    while(max-- > 0)
    {
        int size = 128;
        addr1 = sys_malloc(size); 
        size *= 2; 
        addr2 = sys_malloc(size); 
        size *= 2; 
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        size *= 2; size *= 2; size *= 2; size *= 2; 
        size *= 2; size *= 2; size *= 2; 
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2; 
        addr7 = sys_malloc(size);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
    }

    printk(" k_thread_a(%d), test end\n", sys_getpid());

    while(1)
        ;
}

void k_thread_b(void *arg)
{
    char* para = arg;
    void* addr1;
    void* addr2;
    void* addr3;
    void* addr4;
    void* addr5;
    void* addr6;
    void* addr7;
    void* addr8;
    void* addr9;
    int max = 1000;
     printk(" k_thread_b(%d), test start ... \n", sys_getpid());
    while (max-- > 0) {
        int size = 9;
        addr1 = sys_malloc(size);
        size *= 2; 
        addr2 = sys_malloc(size);
        size *= 2; 
        sys_free(addr2);
        addr3 = sys_malloc(size);
        sys_free(addr1);
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        sys_free(addr5);
        size *= 2; 
        addr7 = sys_malloc(size);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr3);
        sys_free(addr4);

        size *= 2; size *= 2; size *= 2; 
        addr1 = sys_malloc(size);
        addr2 = sys_malloc(size);
        addr3 = sys_malloc(size);
        addr4 = sys_malloc(size);
        addr5 = sys_malloc(size);
        addr6 = sys_malloc(size);
        addr7 = sys_malloc(size);
        addr8 = sys_malloc(size);
        addr9 = sys_malloc(size);
        sys_free(addr1);
        sys_free(addr2);
        sys_free(addr3);
        sys_free(addr4);
        sys_free(addr5);
        sys_free(addr6);
        sys_free(addr7);
        sys_free(addr8);
        sys_free(addr9);
    }
    printk(" k_thread_b(%d), test end\n", sys_getpid());
    
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
