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

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* from linux
 * container_of - cast a member of a structure out to the containing structure
 * 
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 * @ptr:	the pointer to the member.
 */

/* 
#define container_of(type, member, ptr) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
*/

/* 也可以这样定义 */
#define container_of(type, member, ptr) \
    (type *)((size_t)ptr - offsetof(type, member))
 
#endif  /* __LIB_STDDEF_H */
