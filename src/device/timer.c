/* timer.c
 * 配置定时器/计数器，设置时钟中断信号的频率
 */

#include <timer.h>
#include <io.h>
#include <print.h>
#include <thread.h>
#include <debug.h>
#include <interrupt.h>
#include <global.h>

#define IRQ0_FREQUENCY      100     /* 时钟中断频率：100Hz */
#define INPUT_FREQUENCY     1193180 /* 定时器/计数器的工作频率 */
/* 计数初值 */
#define TIMER0_INITIAL_VALUE    INPUT_FREQUENCY / IRQ0_FREQUENCY
#define TIMER0_PORT     0x40    /* 计数器0的端口号 */
#define TIMER0_NO       0       /* 计数器的号码 */
#define TIMER_MODE      2       /* 工作方式为：方式2，比率发生器 */
/* 读写方式：先读写低8位，再读写高8位 */
#define READ_WRITE_LATCH    3
#define PIT_CONTROL_PORT    0x43    /* 控制 寄存器的端口号 */

/* 每多少毫秒发生一次中断 
 * 即：1个时钟周期是10毫秒
 */
#define mil_seconds_per_intr    (1000 / IRQ0_FREQUENCY)

uint32_t ticks;     /* ticks是内核自中断开启以来总共的嘀嗒数 */

/* 初始化模式控制寄存器，并给计数器赋初始值 */
static void set_timer(uint8_t port, uint8_t no, uint8_t rwl,
                    uint8_t mode, uint16_t value)
{
    /* 往控制字寄存器端口0x43中写入控制字 */
    outb(PIT_CONTROL_PORT, (uint8_t)(no << 6 | rwl << 4 | mode << 1));
    /* 先写入计数初值value的低8位 */
    outb(port, (uint8_t)value);
    /* 再写入计数初值value的高8位 */
    outb(port, (uint8_t)value >> 8);
}

/* 时钟中断的中断处理函数 */
static void intr_timer_handler(void)
{
    struct task_struct * cur_thread = running_thread();

    /* 检查栈是否溢出 */
    kassert(cur_thread->stack_magic == STACK_BORDER_MAGIC);

    /* 记录此线程占用的cpu时间嘀嗒数 */
    cur_thread->elapsed_ticks++;

    /* 从内核第一次处理时间中断后开始至今的滴哒数，
     * 内核态和用户态总共的嘀哒数
     */
    ticks++;

    /* 若进程时间片用完就开始调度新的进程上cpu */
    if(cur_thread->ticks == 0)
    {
        schedule();
    }
    else
    {
        /* 将当前进程的时间片-1 */
        cur_thread->ticks--;
    }
}
                        

/* 让任务休眠sleep_ticks个嘀哒
 * 以tick为单位的sleep，任何时间形式的sleep会转换此ticks形式 
 */
static void ticks_to_sleep(uint32_t sleep_ticks)
{  
    uint32_t start_tick = ticks;

    /* 若间隔的ticks数不够便让出cpu */
    while (ticks - start_tick < sleep_ticks)
    {
        thread_yield();
    }
}

/* 以毫秒为单位的sleep   1秒= 1000毫秒 */
void mtime_sleep(uint32_t m_seconds)
{
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    kassert(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}

/* 初始化定时器/计数器 PIT 8253 */
void timer_init(void)
{
    put_str("timer_init ... ");
    /* 设置8253的定时周期,也就是发中断的周期 */
    set_timer(TIMER0_PORT, TIMER0_NO, READ_WRITE_LATCH,
            TIMER_MODE, TIMER0_INITIAL_VALUE);
    register_handler(0x20, intr_timer_handler);
    put_str("ok\n");
}
