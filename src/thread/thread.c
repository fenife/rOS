/* thread.c
 */

#include <thread.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func *func, void *func_arg)
{
    func(func_arg);
}

/* 初始化线程栈thread_stack，将待执行的函数和参数放到thread_stack
 * 中相应的位置
 */
void thread_create(struct task_struct * pthread, thread_func func,
                void * func_arg)
{
    struct thread_stack * kthread_stack;

    /* 先预留中断栈、线程栈的空间 */
    pthread->self_kstack -= sizeof(struct intr_stack);
    pthread->self_kstack -= sizeof(struct thread_stack);

    /* 初始化剩余的部分 */
    kthread_stack = (struct thread_stack *)pthread->self_kstack;
    kthread_stack->eip  = kernel_thread;
    kthread_stack->func = func;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp  = 0;
    kthread_stack->ebx  = 0;
    kthread_stack->esi  = 0;
    kthread_stack->edi  = 0;
}

/* 初始化线程基本信息 */
void thread_init(task_struct * pthread, char * name, int pri)                
{
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);
    pthread->status     = TASK_RUNNINT;
    pthread->priority   = pri;

    /* self_kstack是线程自己在内核态下使用的栈顶地址 
     * 初始化为PCB页的顶端
     */
    pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->stack_magic = STACK_BORDER_MAGIC;
}

/* 创建一个新的线程，
 * 线程名为name，优先级为pri，
 * 线程所执行的函数是func(func_arg)
 */
struct task_struct * thread_start(char * name, int pri, 
        thread_func func, void * func_arg)
{
    /* pcb都位于内核空间，包括用户进程的pcb也是在内核空间 
     * 从内核空间中分配一页来存放pcb相关内容
     */
    struct task_struct * pthread = get_kernel_pages(1);

    thread_init(pthread, name, pri);
    thread_create(pthread, func, func_arg);

    asm volatile ("movl %0, %%esp;  \
            pop %%ebp; pop %%ebx;   \
            pop %%edi; pop %%esi; ret" \
            : : "g"(pthread->self_kstack) \
            : "memory");

    return pthread;
}




