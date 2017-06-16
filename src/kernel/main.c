/* main.c
 *   用实现的put_char()函数打印字符
 */

#include <sys/print.h>

void main(void)
{
    put_str("I am kernel\n");
    put_int(0);
    put_char('\n');
    put_int(9);
    put_char('\n');
    put_int(0x00021a3f);
    put_char('\n');
    put_int(0x12345678);
    put_char('\n');
    put_int(0x0);
    put_char('\n');

    while (1)
        ;
}
