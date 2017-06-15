/* main.c
 *   用实现的put_char()函数打印字符
 */

#include <sys/print.h>

void main(void)
{
    put_char('k');
    put_char('e');
    put_char('r');
    put_char('n');
    put_char('e');
    put_char('l');
    put_char('\n');
    
    put_char('1');
    put_char('2');
    put_char('\b');
    put_char('3');
    
    while (1)
        ;
}
