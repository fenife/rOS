/* main.c
 *   测试kassert宏
 */

#include <sys/print.h>
#include <sys/init.h>
#include <sys/debug.h>

int main(void)
{
    put_str("I am kernel\n");
    init_all();     /* 初始化所有模块 */

    kassert(1 == 2);

    while (1)
        ;

    return 0;
}
