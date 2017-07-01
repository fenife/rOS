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
#include <process.h>
#include <syscall.h>
#include <sys.h>
#include <stdio.h>
#include <memory.h>
#include <timer.h>
#include <fs.h>
#include <dir.h>

void init(void);

int main(void)
{
    put_str("kernel start ... \n");
    init_all();     /* 初始化所有模块 */

    /************ test code start ***************/
    
    
    
    /************ test code end ***************/
    while (1)
        ;

    return 0;
}

/* init进程 */
void init(void)
{
    uint32_t ret_pid = fork();
    if(ret_pid) 
    {
        printf("I am father, my pid is %d, child pid is %d\n", 
                    getpid(), ret_pid);
    } 
    else 
    {
        printf("I am child, my pid is %d, ret pid is %d\n", 
                    getpid(), ret_pid);
    }
    
    while(1)
        ;
}

