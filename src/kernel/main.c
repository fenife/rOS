/* main.c
 *   用实现的put_char()函数打印字符
 */

#include <sys/print.h>
#include <sys/init.h>

void main(void)
{
    put_str("I am kernel\n");
    init_all();     /* 初始化所有模块 */

    asm volatile ("sti");   /* 测试中断处理，临时开中断 */

    while (1)
        ;
}
