/* main.c
 *   测试线程执行
 */

#include <stddef.h>
#include <printk.h>
#include <init.h>
#include <thread.h>
#include <interrupt.h>

void k_thread_a(void *arg);
void k_thread_b(void *arg);


int main(void)
{
    printk("start kernel ... \n");
    init_all();     /* 初始化所有模块 */

    thread_start("k_thread_a", 31, k_thread_a, "arg_a");
    thread_start("k_thread_b", 8, k_thread_b, "arg_b");

    intr_enable();

    while (1)
    {
        printk("main - ");
    }

    return 0;
}

/* 在线程中运行的函数
 * 用void*来通用表示参数，被调用的函数知道自己需要什么类型的参数，
 * 自己转换后再用
 */
void k_thread_a(void *arg)
{
    char *para = (char *)arg;

    while(1)
    {
        printk("%s -", para);
    }
}

void k_thread_b(void *arg)
{
    char *para = (char *)arg;

    while(1)
    {
        printk("%s - ", para);
    }
}

