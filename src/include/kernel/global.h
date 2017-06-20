/* global.h
 *   一些通用的宏定义 
 */

#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H

#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 4

#define TI_GDT 0
#define TI_LDT 1

/* 构建各段的选择子 */
/* 第0个段描述符：不可用 
 * 第1个：代码段，内核级
 * 第2个：数据段和栈段，内核级
 * 第3个：显存，内核级
 */
#define SELECTOR_K_CODE     ((1<<3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA     ((1<<3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK    SELECTOR_K_DATA
#define SELECTOR_K_GS       ((1<<3) + (TI_GDT << 2) + RPL0)

#endif  /* __KERNEL_GLOBAL_H */
