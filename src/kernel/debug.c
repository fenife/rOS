/* debug.c
 * 定义内核调试用的函数
 */

#include <sys/debug.h>
#include <sys/kernel.h>
#include <sys/interrupt.h>

/* 关中断，打印文件名、行号、函数名、条件，并让程序悬停 */
void panic(const char *filename, int line, const char *func_name,
                const char *cond)
{
    /* 关中断，防止屏幕的错误输出信息被其他进程干扰 */
    intr_disable();

    printk("\n\n----------- kernel error! -----------\n");

    printk("filename:  %s\n", filename);
    printk("line:      %d\n", line);
    printk("function:  %s\n", func_name);
    printk("condition: %s\n", cond);

    /* 内核运行中出现问题时，多属于严重错误，没必要再运行下去 */
    while(1)
        ;
}
