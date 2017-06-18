/* string.h
 * 字符串操作函数
 * string functions in c lib
 */

#ifndef __LIB_STRING_H
#define __LIB_STRING_H

void * memchr(const void *s, int c, size_t n);
int    memcmp(const void *s1, const void *s2, size_t n);
void * memcpy(void *s1, const void *s2, size_t n);
void * memmove(void *s1, const void *s2, size_t n);
void * memset(void *s, int c, size_t n);

char * strncat(char *s1, const char *s2, size_t n);
int    strncmp(const char *s1, const char *s2, size_t n);
char * strncpy(char *s1, const char *s2, size_t n);
char * strcat(char *s1, const char *s2);
int    strcmp(const char *s1, const char *s2);
char * strcpy(char *s1, const char *s2);
size_t strlen(const char *s);
char * strchr(const char *s, int c);
char * strrchr(const char *s, int c);
char * strstr(const char *s1, const char *s2);

#endif  /* __LIB_STRING_H */
