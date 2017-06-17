/* timer.c
 * 配置定时器/计数器，设置时钟中断信号的频率
 */

#include <dev/timer.h>
#include <sys/io.h>
#include <sys/print.h>

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

/* 初始化定时器/计数器 PIT 8253 */
void timer_init(void)
{
    put_str("timer_init start ...\n");
    /* 设置8253的定时周期,也就是发中断的周期 */
    set_timer(TIMER0_PORT, TIMER0_NO, READ_WRITE_LATCH,
            TIMER_MODE, TIMER0_INITIAL_VALUE);
    put_str("timer_init done.\n");
}
