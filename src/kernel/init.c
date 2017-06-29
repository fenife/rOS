/* init.c
 * 调用所有模块的初始化主函数
 */

#include <init.h>
#include <interrupt.h>
#include <print.h>
#include <timer.h>
#include <memory.h>
#include <thread.h>
#include <console.h>
#include <keyboard.h>
#include <tss.h>
#include <sys.h>
#include <ide.h>
#include <fs.h>

/* 负责初始化所有模块 */
void init_all(void)
{
    put_str("init_all start ... \n");

    console_init();     /* 初始化控制台 */
    idt_init();         /* 初始化中断 */
    mem_init();         /* 初始化内存管理系统 */
    timer_init();       /* 初始化定时器/计数器，设置时钟中断频率 */
    thread_init();      /* 初始化线程相关结构 */
    keyboard_init();    /* 键盘初始化 */
    tss_init();         /* tss初始化 */
    syscall_init();     /* 初始化系统调用 */
    intr_enable();      /* 后面的ide_init需要打开中断 */
    ide_init();         /* 初始化硬盘 */
    filesys_init();     /* 初始化文件系统 */

    put_str("init_all done.\n");
}
