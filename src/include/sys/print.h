/* print.h
 *   打印函数的声明
 */

#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include <stdint.h>

void put_char(uint8_t ch);
void put_str(char *str);

#endif  /* __LIB_KERNEL_PRINT_H */
