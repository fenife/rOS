/* string.c
 * string functions in c lib
 */

#include <stddef.h>
#include <string.h>
#include <assert.h>

/* stroe c throughout unsigned char s[n]
 * 将s起始的n个字节置为c
 */
void * memset(void *s, int c, size_t n)
{
    assert(NULL != s);

    const unsigned char uc = c;
    unsigned char *su;

    for (su = s; 0 < n; ++su, --n)
        *su = uc;
    return (s);
}

/* copy char s2[n] to s1[n] in any order
 * 将s2起始的n个字节复制到s1
 */
void * memcpy(void *s1, const void *s2, size_t n)
{
    assert(NULL != s1 && NULL != s2);

    char *su1;
    const char *su2;

    for (su1 = s1, su2 = s2; 0 < n; ++su1, ++su2, --n)
        *su1 = *su2;
    return (s1);
}

/* compare unsigned char s1[n], s2[n]
 * 连续比较以地址s1和s2开头的n个字节，若相等返回0，
 * 若s1大于s2，返回+1，否则返回-1
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    assert(NULL != s1 && NULL != s2);

    const unsigned char *su1, *su2;

    for (su1 = s1, su2 = s2; 0 < n; ++su1, ++su2, --n)
        if (*su1 != *su2)
            return ((*su1 < *su2) ? -1 : +1);
    return (0);
}

/* copy char s2[] to s1[] 
 * 将字符串从s2复制到s1
 */
char * strcpy(char *s1, const char *s2)
{
    assert(NULL != s1 && NULL != s2);
    
    char *s = s1;

    for ( s = s1; (*s++ = *s2++) != '\0'; )
        ;

    return (s1);    /* 返回目的字符串的起始地址 */
}

/* find length of s[] 
 * 返回字符串长度
 */
size_t strlen(const char *s)
{
    assert(NULL != s);
    
    const char *sc;

    for (sc = s; *sc != '\0'; ++sc)
        ;

    return (sc - s);
}

/* compare unsigned char s1[] , s2[] 
 * 比较两个字符串，若s1中字符大于s2中的字符，返回1
 * 相等时返回0，否则返回-1
 */
int strcmp(const char *s1, const char *s2)
{
    assert(NULL != s1 && NULL != s2);
    
    for ( ; *s1 == *s2; ++s1, ++s2)
        if (*s1 == '\0')
            return (0);
    return ((*(unsigned char *)s1
        < *(unsigned char *)s2) ? -1 : +1);
}

/* find first occurrence of c in char s[] 
 * 从左到右查找字符串s中首次出现字符c的地址(不是下标,是地址)
 */
char * strchr(const char *s, int c)
{
    assert(NULL != s);
    
    const char ch = c;

    for ( ; *s != ch; ++s)
        if ( *s == '\0')
            return (NULL);
    return ((char *)s);
}

/* find last occurrence of c in char s[] 
 * 从后往前查找字符串s中首次出现字符c的地址(不是下标,是地址)
 */
char * strrchr(const char *s, int c)
{
    assert(NULL != s);
    
    const char ch = c;
    const char *sc;

    for (sc = NULL; ; ++s)
    {
        if (*s == ch)
            sc = s;
        if (*s == '\0')
            return ((char *)sc);
    }
}

/* copy char s2[] to end of s1[] 
 * 将字符串s2拼接到s1后，并返回拼接后字符串的地址
 */
char * strcat(char *s1, const char *s2)
{
    assert(NULL != s1 && NULL != s2);
    
    char *s;

    /* find end of s1[] */
    for (s = s1; *s != '\0'; s++)
        ;

    /* copy s2[] to end */
    for ( ; (*s = *s2) != '\0'; ++s, ++s2)
        ;

    return (s1);
}


/* copy char s2[max n] to end of s1[] */
char * strncat(char *s1, const char *s2, size_t n)
{
    assert(NULL != s1 && NULL != s2);
    
    char *s;

    /* find end of s1[] */
    for (s = s1; *s != '\0'; ++s)
        ;

    /* copy at most n chars for s2[] */
    for( ; 0 < n && *s2 != '\0'; --n)
        *s++ = *s2++;
    *s = '\0';
    return (s1);
}

/* compare unsigned char s1[max n], s2[max n] */
int strncmp(const char *s1, const char *s2, size_t n)
{
    assert(NULL != s1 && NULL != s2);
    
    for ( ; 0 < n; ++s1, ++s2, --n)
        if (*s1 != *s2)
            return ((*(unsigned char *)s1
                < *(unsigned char *)s2) ? -1 : +1);
        else if (*s1 == '\0')
            return (0);
    return (0);
}

/* copy char s2[max n] to s1[n] */
char * strncpy(char *s1, const char *s2, size_t n)
{
    assert(NULL != s1 && NULL != s2);
    
    char *s;

    /* copy at most n chars from s2[] */
    for (s = s1; 0 < n && *s2 != '\0'; --n)
        *s++ = *s2++;

    for ( ; 0 < n; --n)
        *s++ = '\0';
    return (s1);
}



/* find first occurrence of s2[] in s1[] */
char * strstr(const char *s1, const char *s2)
{
    assert(NULL != s1 && NULL != s2);
    
    if (*s2 == '\0')
        return ((char *)s1);

    for ( ; (s1 = strchr(s1, *s2)) != NULL; ++s1 )
    {
        const char *sc1, *sc2;

        for (sc1 = s1, sc2 = s2; ; )
            if (*++s2 == '\0')
                return ((char *)s1);
            else if (*++sc1 != *sc2)
                break;
    }
    return (NULL);
}

/* find first occurrence of c in s[n] */
void * memchr(const void *s, int c, size_t n)
{
    assert(NULL != s);

    const unsigned char uc = c;
    const unsigned char *su;

    for (su = s; 0 < n; ++su, --n)
        if (*su == uc)
            return ((void *)su);
    return (NULL);
}


/* copy char s2[n] to s1[n] safely */
void * memmove(void *s1, const void *s2, size_t n)
{
    assert(NULL != s1 && NULL != s2);
    
    char * sc1;
    const char *sc2;

    sc1 = s1;
    sc2 = s2;

    if (sc2 < sc1 && sc1 < sc2 + n)
        for (sc1 += n, sc2 += n; 0 < n; --n)
            *--sc1 = *--sc2;    /* copy backwards */
    else
        for (; 0 < n; --n)
            *sc1++ = *sc2++;    /* copy forwards */
    return (s1);
}
