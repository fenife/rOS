/* interrupt.c
 *   中断初始化，创建中断描述符表
 */

#include <stdint.h>
#include <sys/global.h>
#include <sys/interrupt.h>
#include <sys/io.h>
#include <sys/print.h>

/* 这里用的可编程中断控制器是8259A */
#define PIC_M_CTRL  0x20    /* 主片的控制端口是0x20 */
#define PIC_M_DATA  0x21    /* 主片的数据端口是0x21 */
#define PIC_S_CTRL  0xa0    /* 从片的控制端口是0xa0 */
#define PIC_S_DATA  0xa1    /* 从片的数据端口是0xa1 */

static void create_idt_desc(struct intr_desc *desc,
        uint8_t attr, intr_handler handler);

/* 中断描述符表，本质上就是个中断门描述符数组 */
static struct intr_desc idt[IDT_DESC_CNT];

/* 声明引用定义在kernel.S中的中断处理函数入口数组 */
extern intr_handler intr_entry_table[IDT_DESC_CNT];

/* 初始化可编程中断控制器8259A */
static void pic_init(void)
{
    /* 初始化主片 */
    outb(PIC_M_CTRL, 0x11); /* ICW1: 边沿触发，级联8259，需要ICW4 */
    /* ICW2: 起始中断向量号为0x20，也就是IR[0-7]为 0x20 ~ 0x27 */
    outb(PIC_M_DATA, 0x20);
    outb(PIC_M_DATA, 0x04); /* ICW3: IR2接从片 */
    outb(PIC_M_DATA, 0x01); /* ICW4: 8086模式，正常EOI */

    /* 初始化从片 */
    outb (PIC_S_CTRL, 0x11);	/* ICW1: 边沿触发,级联8259，需要ICW4 */
    /* ICW2: 起始中断向量号为0x28，也就是IR[8-15]为 0x28 ~ 0x2F */
    outb (PIC_S_DATA, 0x28);
    outb (PIC_S_DATA, 0x02);	/* ICW3: 设置从片连接到主片的IR2引脚 */
    outb (PIC_S_DATA, 0x01);	/* ICW4: 8086模式，正常EOI */

    /* 打开主片上IR0，也就是目前只接受时钟产生的中断 */
    outb(PIC_M_DATA, 0xfe);
    outb(PIC_S_DATA, 0xff);

    put_str("   pic_init_done\n");
}

/****************************************************
 * 函数描述：创建中断门描述符
 * 输入参数：
 *   @attr：定义各描述符的属性值
 *   @handler：中断处理程序的地址
 * 输出参数：
 *   @desc：中断描述符
 ****************************************************/
static void create_idt_desc(struct intr_desc * desc,
        uint8_t attr, intr_handler handler)
{
    desc->offset_low  = (uint32_t)handler & 0x0000FFFF;
    desc->selector    = SELECTOR_K_CODE;
    desc->dcount      = 0;
    desc->attr        = attr;
    desc->offset_high = ((uint32_t)handler & 0xFFFF0000) >> 16;
}

/* 初始化中断描述符表 */
static void idt_desc_init(void)
{
    int i;

    for (i = 0; i < IDT_DESC_CNT; i++)
    {
        create_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, 
                    (intr_handler)intr_entry_table[i]);
    }

    put_str("   idt_desc_init done\n");
}

/* 完成与中断有关的所有初始化工作 */
void idt_init(void)
{
    put_str("idt_init start ... \n");
    idt_desc_init();    /* 初始化中断描述符表 */
    pic_init();         /* 初始化8259A */

    /* 加载idt，前16位为表界限，后32位为表基址
     * 因为C语言中没有48位的数据类型，这里用64位来代替
     * 只要保证64位变量的前48位数据正确就可以
     *
     * 32位的指针只能转换成相同大小的整型，不能直接转换成64位的整型
     * idt是指针类型，用迂回的方法，先转换成32位型，再转换成64位整型，
     * 最后进行16位左移，会丢弃64位整型数的最高的16位
     */
    uint64_t idt_operand = ((sizeof(idt) -1)
        | ((uint64_t)(uint32_t)idt) << 16);

    /* lidt只从内存地址处获取其中的48位数据当作操作数
     * "m"是内存约束
     * %0 其实是变量idt_operand的地址&idt_operand，
     *    但AT&T语法把数字当成内存地址，所以直接从该地址处取得数据
     */
    asm volatile("lidt %0": : "m"(idt_operand));

    put_str("idt_init done\n");
}
