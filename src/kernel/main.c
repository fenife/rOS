/* main.c
 *   用实现的put_char()函数打印字符
 */

#include <sys/print.h>

void main(void)
{
    put_str("I am kernel\n");

    while (1)
        ;
}
