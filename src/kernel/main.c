/* main.c
 *   测试kassert宏
 */

#include <stddef.h>
#include <sys/print.h>
#include <sys/init.h>
#include <string.h>
#include <sys/kernel.h>

int main(void)
{
    char buf[20];
    size_t len = 0;

    put_str("I am kernel\n");
    init_all();     /* 初始化所有模块 */

    memset(buf, 0, 20);
    strcpy(buf, "hello world\n");
    len = strlen(buf);

    printk("%s, len = %d\n", buf, len);

    while (1)
        ;

    return 0;
}
