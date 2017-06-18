/* kernel.h 
 */

#ifndef __KERNEL_KERNEL_H
#define __KERNEL_KERNEL_H

#include <stdarg.h>

int printk(const char *fmt, ...);

int vsprintf(char *buf, const char *fmt, va_list args);

#endif  /* __KERNEL_KERNEL_H */