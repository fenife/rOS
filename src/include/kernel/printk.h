/* printk.h
 */

#ifndef __KERNEL_PRINTK_H
#define __KERNEL_PRINTK_H

#include <stdint.h>

#define KDEBUG

uint32_t printk(const char *fmt, ...);

#ifdef KDEBUG       /* kernel debug */
#define dbg(...) {   \
        printk("%s(%s)[%d] -- ", __FILE__, __func__, __LINE__); \
        printk(__VA_ARGS__);   }
#else
#define dbg(...)
#endif

#endif  /* __KERNEL_PRINTK_H */
