/* list.c
 */

#include <list.h>
#include <interrupt.h>

/* 初始化双向链表 */
void list_init(struct list * list)
{
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

/* 把链表结点元素elem插入到元素before之前 */
void list_insert(struct node * before, struct node * elem)
{
    /* 保存中断状态，并关中断，保证下面操作的原子性 */
    intr_status old_status = intr_disable();

    /* 将before前驱元素的后继元素更新为elem，暂时使before脱离链表 */
    before->prev->next = elem;

    /* 更新elem自己的前驱结点为before的前驱，
     * 更新elem自己的后继结点为before，于是before又回到链表
     */
    elem->prev = before->prev;
    elem->next = before;

    /* 更新before的前驱结点为elem */
    before->prev = elem;

    /* 恢复中断状态 */
    intr_set_status(old_status);
}

/* 从链表中删除结点元素elem */
void list_remove(struct node * pelem)
{
    intr_status old_status = intr_disable();

    pelem->prev->next = pelem->next;
    pelem->next->prev = pelem->prev;
    
    intr_set_status(old_status);
}

/* 添加元素到列表队首，类似栈push操作 */
void list_push(struct list *plist, struct node * elem)
{
    list_insert(plist->head.next, elem);    /* 在链表头插入elem */
}

/* 将链表第一个元素弹出并返回，类似栈的pop操作 */
struct node * list_pop(struct list * plist)
{
    struct node * elem = plist->head.next;
    list_remove(elem);
    return elem;
}

/* 追加结点元素到链表队尾，类似队列的先进先出操作 */
void list_append(struct list * plist, struct node * elem)
{
    list_insert(&plist->tail, elem);
}

/* 从链表中查找结点元素target，
 * 成功时返回true，失败时返回false
 */
bool find_elem(struct list *plist, struct node * target)
{
    struct node * pelem = plist->head.next;

    while(pelem != &plist->tail)
    {
        if (pelem == target)
        {
            return true;
        }
        pelem = pelem->next;
    }
    return false;
}

/* 本函数的功能是遍历列表内所有元素，逐个判断是否有符合条件的元素，
 * 找到符合条件的元素返回元素指针，否则返回NULL。
 *
 * 把列表plist中的每个元素elem和条件cond传给回调函数check，
 * cond给check用来判断elem是否符合条件。
 */
struct node * list_traversal(struct list * plist, 
            check_elem check, int cond)
{
    struct node * pelem = plist->head.next;

    /* 如果队列为空，就必然没有符合条件的结点，直接返回NULL */
    if (list_empty(plist))
    {
        return NULL;
    }

    while (pelem != &plist->tail)
    {
        /* check_elem返回ture则认为该元素在回调函数中符合条件，
         * 命中，故停止继续遍历 
         */
        if (check(pelem, cond))
        {
            return pelem;
        }
        pelem = pelem->next;
    }

    return NULL;
}

/* 返回链表长度 */
uint32_t list_len(struct list *plist)
{
    struct node * pelem = plist->head.next;
    uint32_t length = 0;

    while (pelem != &plist->tail)
    {
        length++;
        pelem = pelem->next;
    }
    return length;
}                

/* 判断链表是否为空，空时返回true，否则返回false */
bool list_empty(struct list *plist)
{
    return (plist->head.next == &plist->tail ? true : false);
}
