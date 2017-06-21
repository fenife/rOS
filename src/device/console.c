/* console.c
 */

#include <console.h>
#include <sync.h>
#include <print.h>
#include <thread.h>

/* 控制台锁 */
static struct lock console_lock;

/* 初始化终端 */
void console_init(void)
{
    lock_init(&console_lock);
}

/* 获取终端 */
void console_acquire(void)
{
    lock_acquire(&console_lock);
}

/* 释放终端 */
void console_release(void)
{
    lock_release(&console_lock);
}

/* 终端中输出字符串 */
void console_put_str(char *str)
{
    console_acquire();
    put_str(str);
    console_release();
}

/* 终端中输出字符 */
void console_put_char(uint8_t ch)
{
    console_acquire();
    put_char(ch);
    console_release();
}

/* 终端中输出16进制整数 */
void console_put_int(uint32_t num)
{
    console_acquire();
    put_int(num);
    console_release();
}
