/* printk.c
 * from linux
 */

#include <stdarg.h>
#include <console.h>
#include <printk.h>

static char buf[1024];

int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf,fmt,args);
	va_end(args);
	console_put_str(buf);
	return i;
}

