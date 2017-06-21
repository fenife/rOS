/* list.h
 */

#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include <stddef.h>
#include <stdint.h>

/**********   定义链表结点成员结构   ***********
* 结点中不需要数据成元,只要求前驱和后继结点指针
**********************************************/
struct node {
    struct node * prev;     /* 前驱结点 */
    struct node * next;     /* 后继结点 */
};

/* 链表结构，用来实现队列
 * head是队首，是固定不变的，但不是第1个元素，第1个元素为head.nex
 * tail是队尾，同样是固定不变的，链表的最后一个节点是tail.prev
 */
struct list {
    struct node head;
    struct node tail;
};

/* 自定义函数类型check_elem,用于在list_traversal中做回调函数
 * 检查链表结点元素是否符合某种条件
 */
typedef bool (check_elem)(struct node * elem, int cond);

void list_init(struct list *list);
void list_insert(struct node * before, struct node * elem);
void list_remove(struct node * pelem);
void list_push(struct list *plist, struct node * elem);
struct node * list_pop(struct list * plist);
void list_append(struct list * plist, struct node * elem);
bool elem_find(struct list *plist, struct node * target);
struct node * list_traversal(struct list * plist, 
            check_elem check, int cond);
uint32_t list_len(struct list *plist);
bool list_empty(struct list *plist);

#endif  /* __LIB_KERNEL_LIST_H */
