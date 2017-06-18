/* interrupt.c
 *   中断初始化，创建中断描述符表，可自由开关中断
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

/* 用于获取中断状态 */
/* 开中断时eflags寄存器的if位为1 */
#define EFLAGES_IF  0x00000200

/* 获取eflags寄存器的值，保存在变量var中，
 * 寄存器约束 g 表示var可以放到内存或寄存器中
 */
#define GET_EFALGS(var) asm volatile ("pushfl; popl %0" : "=g"(var))


static void create_idt_desc(struct intr_desc *desc,
        uint8_t attr, intr_handler handler);

/* 中断描述符表，本质上就是个中断门描述符数组 */
static struct intr_desc idt[IDT_DESC_CNT];

/* 中断异常名数组，用于保存每一项异常的名字，将来调试时用 */
char* intr_name[IDT_DESC_CNT];

/* 中断处理程序数组，在kernel.S中定义的intr_xx_entry只是
 * 中断处理程序的入口，最终调用的是此数组中的处理程序
 */
intr_handler intr_handler_table[IDT_DESC_CNT];

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

/* 通用的中断处理函数，一般用在异常出现时的处理 */
static void general_intr_handler(uint8_t vec_nr)
{
    /* IRQ7和IRQ15会产生伪中断(spurious interrupt)，无须处理
     * 0x2f是从片8259A上的最后一个irq引脚，保留
     */
    if (0x27 == vec_nr || 0x2f == vec_nr)
        return;

    put_str("int vector: 0x");
    put_int(vec_nr);
    put_char('\n');
}

/* 完成一般中断处理函数注册及异常名称注册 */
static void exception_init(void)
{
    int i = 0;

    /* intr_handler_table数组中的函数是在进入中断后根据中断向量号调用的,
     * 见kernel/kernel.S的call [idt_table + %1*4]
     *
     * 先设置为默认的中断处理程序：general_intr_handler
     * 以后会由register_handler来注册具体的中断处理函数
     */
    for (i = 0; i < IDT_DESC_CNT; i++)
    {
        intr_handler_table[i] = general_intr_handler;
        intr_name[i] = "unknown";       /* 先统一赋值为unknown */
    }

    /* 为0~19这20个异常赋予正确的异常名称
     * 将来在异常出现时，可以根据中断向量号在此数组中检索到
     * 具体的异常名，以排查错误
     */
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    /* intr_name[15] 第15项是intel保留项，未使用 */
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";

    put_str("   exception_init done\n");
}

/* 完成与中断有关的所有初始化工作 */
void idt_init(void)
{
    put_str("idt_init start ... \n");
    idt_desc_init();    /* 初始化中断描述符表 */
    exception_init();   /* 异常名初始化并注册通常的中断处理函数 */
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

/* 开中断，并返回开中断前的状态 */
intr_status intr_enable(void)
{
    if (INTR_ON == intr_get_status())
    {
        return INTR_ON;
    }
    else
    {
        asm volatile ("sti");   /* 开中断，sti指令将IF位置1 */
        return INTR_OFF;
    }
}

/* 关中断，并返回关中断前的状态 */
intr_status intr_disable(void)
{
    if (INTR_ON == intr_get_status())
    {
        /* 关中断，cli指令将IF位置0 */
        asm volatile ("cli" : : : "memory");
        return INTR_ON;
    }
    else
    {
        return INTR_OFF;
    }
}

/* 将中断状态设置为status */
intr_status intr_set_status(intr_status status)
{
    return (status && INTR_ON) ? intr_enable() : intr_disable();
}

/* 获取当前中断状态 */
intr_status intr_get_status(void)
{
    uint32_t eflags = 0;

    GET_EFALGS(eflags);
    return (EFLAGES_IF & eflags) ? INTR_ON : INTR_OFF;
}
