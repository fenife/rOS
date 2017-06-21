/* print.h
 *   打印函数的声明
 */

#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include <stdint.h>

void put_char(uint8_t ch);
void put_str(char *str);
void put_int(uint32_t num);     /* 以十六进制打印数字 */
void set_cursor(uint32_t cursor_pos);

#endif  /* __LIB_KERNEL_PRINT_H */
