/* interrupt.h
 *   和中断相关的声明
 */

#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H

#include <stdint.h>

/* 目前总共支持的中断数 */
#define IDT_DESC_CNT    33

/* --------------   IDT描述符属性       ------------ */
#define IDT_DESC_P      1
#define IDT_DESC_DPL0   0
#define IDT_DESC_DPL3   3
/* 32位的门 */
#define IDT_DESC_32_TYPE    0xE     /* 1110b */
/* 16位的门，不用，定义它只为和32位门区分 */
#define IDT_DESC_16_TYPE    0x6     /* 0110b */

#define IDT_DESC_ATTR_DPL0  \
    ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)

#define IDT_DESC_ATTR_DPL3 \
    ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

/* -------------- 中断门描述符结构体 ------------ */
struct intr_desc {
    uint16_t offset_low;    /* 中断处理程序在目标代码段内的偏移量的低16位 */
    uint16_t selector;      /* 中断处理程序目标代码段的选择子 */
    uint8_t  dcount;        /* 此项是描述符的第4字节，为固定值，不用考虑 */
    uint8_t  attr;          /* 此项是描述符的第5字节，定义各属性值 */
    uint16_t offset_high;   /* 中断处理程序在目标代码段内的偏移量的高16位 */
};

/* 类型定义 */
typedef void* intr_handler;

/* 定义中断的两种状态 */
typedef enum
{
    INTR_OFF = 0,       /* 0表示关中断 */
    INTR_ON             /* 1表示开中断 */
} intr_status;

/* 函数声明 */
void idt_init(void);

intr_status intr_get_status(void);
intr_status intr_set_status(intr_status status);
intr_status intr_enable(void);
intr_status intr_disable(void);

#endif  /* __KERNEL_INTERRUPT_H */
