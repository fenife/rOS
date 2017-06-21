/* sync.h
 */

#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include <list.h>
#include <stdint.h>
#include <thread.h>

/* 信号量结构 */
typedef struct semaphore 
{
    uint8_t value;
    /* 在此信号量上阻塞的队列 */
    struct list waiters;
} semaphore; 

/* 锁结构 */
typedef struct lock 
{
    struct   task_struct * holder;  /* 锁的持有者 */
    struct   semaphore semaphore;   /* 用二元信号量实现锁 */
    uint32_t holder_repeat_nr;      /* 锁的持有者重复申请此锁的次数 */
} lock;

void sema_init(struct semaphore * psema, uint8_t value);
void sema_down(struct semaphore * psema);
void sema_up(struct semaphore * psema);
void lock_init(struct lock * plock);
void lock_acquire(struct lock * plock);
void lock_release(struct lock * plock);

#endif  /* __THREAD_SYNC_H */