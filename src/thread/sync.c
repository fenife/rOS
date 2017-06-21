/* sync.c
 */
#include <sync.h>
#include <interrupt.h>
#include <debug.h>

/* 初始化信号量 */
void sema_init(struct semaphore * psema, uint8_t value)
{
    psema->value = value;   /* 为信号量赋初值 */
    list_init(&psema->waiters);     /* 初始化信号量的阻塞队列 */
}

/* 初始化锁plock */
void lock_init(struct lock * plock)
{
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);
}

/* 信号量down操作 */
void sema_down(struct semaphore * psema)
{
    /* 关中断来保证原子操作 */
    intr_status old_status = intr_disable();

    /* 若value为0,表示已经被别人持有
     * 用while不用if的原因，线程在被唤醒后要重新判断
     */
    while(psema->value == 0)
    {
        kassert(!elem_find(&psema->waiters, &running_thread()->general_tag));

        /* 当前线程不应该已在信号量的waiters队列中 */
        if (elem_find(&psema->waiters, &running_thread()->general_tag))
        {
            PANIC("sema_down: thread blocked has been in waiters_list\n");
        }

        /* 若信号量的值等于0，则当前线程把自己加入该锁的等待队列，
         * 然后阻塞自己
         */
        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }

    /* 若value为1或被唤醒后，会执行下面的代码，也就是获得了锁 */
    psema->value--;
    kassert(psema->value == 0);

    /* 恢复之前的中断状态 */
    intr_set_status(old_status);
}

/* 信号量的up操作 */
void sema_up(struct semaphore * psema)
{
    /* 关中断，保证原子操作 */
    intr_status old_status = intr_disable();

    kassert(psema->value == 0);
    if (!list_empty(&psema->waiters))
    {
        struct task_struct * thread_blocked = container_of(
                struct task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;
    kassert(psema->value == 1);

    /* 恢复之前的中断状态 */
    intr_set_status(old_status);
}

/* 获取锁plock */
void lock_acquire(struct lock * plock)
{
    /* 排除曾经自己已经持有锁但还未将其释放的情况 */
    if (plock->holder != running_thread())
    {
        /* 对信号量P（减）操作，原子操作 */
        sema_down(&plock->semaphore);   
        plock->holder = running_thread();
        kassert(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    }
    else
    {
        plock->holder_repeat_nr++;
    }
}

/* 释放锁plock */
void lock_release(struct lock * plock)
{
    kassert(plock->holder == running_thread());
    if (plock->holder_repeat_nr > 1)
    {
        plock->holder_repeat_nr--;
        return;
    }
    kassert(plock->holder_repeat_nr == 1);

    plock->holder = NULL;   /* 把锁的持有者置空放在V（增）操作之前 */
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);
}
