/* stddef.h
 */

#ifndef __LIB_STDDEF_H
#define __LIB_STDDEF_H

#define NULL ((void *)0)

typedef enum
{
    FALSE = 0,
    TRUE
} bool;

typedef signed int      ssize_t;
typedef unsigned int    size_t;

#endif  /* __LIB_STDDEF_H */
