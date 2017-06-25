/* stdio.c
 */

#include <stdarg.h>
#include <stdio.h>
#include <syscall.h>

extern int vsprintf(char *buf, const char *fmt, va_list args);

uint32_t printf(const char *fmt, ...)
{
	va_list args;
	int i;
    char buf[1024] = { 0 };

	va_start(args, fmt);
	i = vsprintf(buf,fmt,args);
	va_end(args);
	write(buf);

	return i;
}

uint32_t sprintf(char * buf, const char * fmt, ...)
{
    va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf,fmt,args);
	va_end(args);

	return i;
}
