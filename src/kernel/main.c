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
#include <shell.h>
#include <assert.h>

void init(void);

int main(void)
{
    put_str("kernel start ... \n");
    init_all();     /* 初始化所有模块 */

    /************ test code start ***************/
    
    cls_screen();
    console_put_str("[user@localhost /]$ ");
    
    /************ test code end ***************/
    while (1)
        ;

    return 0;
}

/* init进程 */
void init(void)
{
    uint32_t ret_pid = fork();
    if(ret_pid)     /* 父进程 */
    {
        printf("I am father, my pid is %d, child pid is %d\n", 
                    getpid(), ret_pid);
        while(1)
            ;
    } 
    else    /* 子进程 */
    {
        printf("I am child, my pid is %d, ret pid is %d\n", 
                    getpid(), ret_pid);
        my_shell();
    }
    
    panic("init: should not be here\n");
}

