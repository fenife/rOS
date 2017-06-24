/* printk.c
 * from linux
 */

#include <stdarg.h>
#include <console.h>
#include <printk.h>
#include <print.h>

extern int vsprintf(char *buf, const char *fmt, va_list args);

uint32_t printk(const char *fmt, ...)
{
	va_list args;
	int i;
    char buf[1024] = { 0 };

	va_start(args, fmt);
	i = vsprintf(buf,fmt,args);
	va_end(args);
	console_put_str(buf);

	return i;
}
