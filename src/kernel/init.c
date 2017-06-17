/* init.c
 * 调用所有模块的初始化主函数
 */

#include <sys/init.h>
#include <sys/interrupt.h>
#include <sys/print.h>
#include <dev/timer.h>

/* 负责初始化所有模块 */
void init_all(void)
{
    put_str("init_all start ... \n");
    idt_init();     /* 初始化中断 */
    timer_init();   /* 初始化定时器/计数器，设置时钟中断频率 */
    put_str("init_all done.\n");
}
