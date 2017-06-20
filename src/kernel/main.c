/* main.c
 *   测试kassert宏
 */

#include <stddef.h>
#include <print.h>
#include <init.h>
#include <string.h>
#include <kernel.h>
#include <debug.h>
#include <memory.h>

int main(void)
{
    put_str("start kernel ... \n");
    init_all();     /* 初始化所有模块 */

    void * addr = get_kernel_pages(3);
    printk("\n - get_kernel_pages start vaddr is : 0x%x\n",
        (uint32_t)addr);

    while (1)
        ;

    return 0;
}
