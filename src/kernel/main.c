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

void k_thread_a(void *arg);
void k_thread_b(void *arg);


int main(void)
{
    put_str("start kernel ... \n");
    init_all();     /* 初始化所有模块 */

    thread_start("consumer_a", 31, k_thread_a, " A_");
    thread_start("consumer_b", 31, k_thread_b, " B_");

    intr_enable();

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
    while(1)
    {
        intr_status old_status = intr_disable();
        if (!ioq_empty(&kbd_buf))
        {
            printk(arg);
            char byte = ioq_getchar(&kbd_buf);
            printk("%c", byte);
        }
        intr_set_status(old_status);
    }
}

void k_thread_b(void *arg)
{
    while(1)
    {
        intr_status old_status = intr_disable();
        if (!ioq_empty(&kbd_buf))
        {
            printk(arg);
            char byte = ioq_getchar(&kbd_buf);
            printk("%c", byte);
        }
        intr_set_status(old_status);
    }
}
