/* printk.h 
 */

#ifndef __KERNEL_PRINTK_H
#define __KERNEL_PRINTK_H

#include <stdarg.h>

int printk(const char *fmt, ...);

int vsprintf(char *buf, const char *fmt, va_list args);

#ifdef KDEBUG       /* kernel debug */
#define dbg(...) {   \
        printk("%s(%s)[%d] -- ", __FILE__, __func__, __LINE__); \
        printk(__VA_ARGS__);   }
#else
#define dbgdbg(...) 
#endif

#endif  /* __KERNEL_PRINTK_H */