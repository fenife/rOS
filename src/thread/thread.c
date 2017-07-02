/* thread.c
 */

#include <thread.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>
#include <interrupt.h>
#include <debug.h>
#include <printk.h>
#include <process.h>
#include <print.h>
#include <sync.h>
#include <global.h>
#include <file.h>
#include <stdio.h>

struct task_struct * main_thread;       /* 主线程PCB */
struct task_struct * idle_thread;       /* idle线程 */
struct list thread_ready_list;          /* 就绪队列 */
struct list thread_all_list;            /* 所有任务队列 */
struct lock pid_lock;                   /* 分配pid锁 */
static struct node * thread_tag;        /* 用于保存队列中的线程结点 */

extern void switch_to(struct task_struct * cur, struct task_struct *next);
extern void init(void);


/* 系统空闲时运行的线程 */
static void idle(void *arg UNUSED)
{
    while (1)
    {
        thread_block(TASK_BLOCKED);

        /* 执行hlt时必须要保证目前处在开中断的情况下 */
        asm volatile ("sti; hlt" : : : "memory");
    }
}


/* 获取当前进程的PCB指针，即PCB页表基址
 *
 * 各个线程所用的0级栈都是在自己的PCB当中，因此取当前栈指针
 * (esp)的高20位即PCB所在页表的基址，亦即PCB的起始地址
 */
struct task_struct * running_thread(void)
{
    uint32_t esp;

    asm volatile ("mov %%esp, %0" : "=g"(esp));
    return (struct task_struct *)(esp & 0xfffff000);
}

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func *func, void *func_arg)
{
    /* 执行function前要开中断，避免后面的时钟中断被屏蔽，
     * 而无法调度其它线程
     */
    intr_enable();

    func(func_arg);
}

/* 分配pid */
static pid_t allocate_pid(void)
{
    static pid_t next_pid = 0;
    lock_acquire(&pid_lock);
    next_pid++;
    lock_release(&pid_lock);

    return next_pid;
}


/* fork进程时为其分配pid，因为allocate_pid已经是静态的，别的文件无法调用
 * 不想改变函数定义了，故定义fork_pid函数来封装一下
 */
pid_t fork_pid(void) 
{
    return allocate_pid();
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
void init_thread(task_struct * pthread, char * name, int pri)
{
    memset(pthread, 0, sizeof(*pthread));
    pthread->pid = allocate_pid();
    strcpy(pthread->name, name);

    /* 由于把main函数也封装成一个线程，并且它一直是运行的，
     * 故将其状态直接设为TASK_RUNNING
     */
    if (pthread == main_thread)
    {
        pthread->status = TASK_RUNNING;
    }
    else
    {
        pthread->status = TASK_READY;
    }

    /* self_kstack是线程自己在内核态下使用的栈顶地址
     * 初始化为PCB页的顶端
     */
    pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);
    pthread->priority    = pri;

    /* 任务运行时间由优先级决定，优先级越高，ticks越大 */
    pthread->ticks       = pri;

    pthread->elapsed_ticks = 0;
    pthread->pgdir       = NULL;

    /* 预留标准输入、标准输出、标准错误 */
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;

    /* 其余的全置为-1 */
    uint8_t fd_idx = 0;
    while (fd_idx < MAX_FILES_OPEN_PER_PROC) 
    {
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }

    pthread->cwd_inode_nr = 0;	    /* 以根目录做为默认工作路径 */
    pthread->parent_pid = -1;       /* 1表示没有父进程 */
    
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

    init_thread(pthread, name, pri);
    thread_create(pthread, func, func_arg);

    /* 加入就绪线程队列，并确保此队列之前并没有此线程 */
    kassert(!elem_find(&thread_ready_list, &pthread->general_tag));
    list_append(&thread_ready_list, &pthread->general_tag);

    /* 加入全部线程队列，并确保此队列之前并没有此线程 */
    kassert(!elem_find(&thread_all_list, &pthread->all_list_tag));
    list_append(&thread_all_list, &pthread->all_list_tag);

    return pthread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void)
{
    /* 因为main线程早已运行，在loader.S中进入内核时有
     * mov esp,0xc009f000，此时已为其预留了tcb，地址
     * 为0xc009e000，因此不需要通过get_kernel_page另分配一页
     */
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    /* main函数是当前线程,当前线程不在thread_ready_list中，
     * 所以只将其加在thread_all_list中
     */
    kassert(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

/* 实现任务调度 */
void schedule(void)
{
    kassert( INTR_OFF == intr_get_status());

    struct task_struct * cur = running_thread();

    /* 若此线程只是cpu时间片到了，将其加入到就绪队列尾 */
    if (TASK_RUNNING == cur->status)
    {
        kassert(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);

        /* 重新将当前线程的ticks再重置为其priority */
        cur->ticks = cur->priority;

        cur->status = TASK_READY;
    }
    else
    {
        /* 若此线程需要某事件发生后才能继续上cpu运行，
         * 不需要将其加入队列，因为当前线程不在就绪队列中
         */
    }

    /* 如果就绪队列中没有可运行的任务，就唤醒idle */
    if (list_empty(&thread_ready_list))
    {
        thread_unblock(idle_thread);
    }
    
    kassert(!list_empty(&thread_ready_list));
    thread_tag = NULL;

    /* 将thread_ready_list队列中的第一个就绪线程弹出，
     * 准备将其调度上cpu
     */
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct * next = container_of(struct task_struct,
            general_tag, thread_tag);
    next->status = TASK_RUNNING;

    /* 击活任务页表等 */
    process_activate(next);

    switch_to(cur, next);
}

/* 当前进程主动将自己阻塞，标志其状态为stat */
void thread_block(task_status stat)
{
    /* stat取值为TASK_BLOCKED、TASK_WAITING、TASK_HANGING之一，
     * 也就是只有这三种状态才不会被调度
     */
    kassert(TASK_BLOCKED == stat || TASK_WAITING == stat
            || TASK_HANGING == stat);

    intr_status old_status = intr_disable();
    struct task_struct * cur_thread = running_thread();
    cur_thread->status = stat;  /* 置其状态为stat */

    /* 将当前线程换下处理器，重新调度下一个任务 */
    schedule();

    /* 待当前线程被解除阻塞后才继续运行下面的intr_set_status */
    intr_set_status(old_status);
}

/* 将线程pthread解除阻塞 */
void thread_unblock(struct task_struct * pthread)
{
    intr_status old_status = intr_disable();

    kassert(TASK_BLOCKED == pthread->status || TASK_WAITING == pthread->status
            || TASK_HANGING == pthread->status);

    if (pthread->status != TASK_READY)
    {
        kassert(!elem_find(&thread_ready_list, &pthread->general_tag));
        if (elem_find(&thread_ready_list, &pthread->general_tag))
        {
            PANIC("thread_unblock: blocked thread in ready_list\n");
        }

        /* 放到队列的最前面，使其尽快得到调度 */
        list_push(&thread_ready_list, &pthread->general_tag);
        pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

/* 主动让出cpu，换其它线程运行 */
void thread_yield(void)
{
    struct task_struct * cur = running_thread();
    intr_status old_status = intr_disable();
    kassert(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}


/* 以填充空格的方式对齐输出buf */
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) 
{
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;
    switch(format) 
    {
        case 's':
            out_pad_0idx = sprintf(buf, "%s", ptr);
            break;

        case 'd':
            out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));

        case 'x':
            out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
    }
    while(out_pad_0idx < buf_len) 
    { 
        /* 以空格填充 */
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no, buf, buf_len - 1);
}


/* 此函数用作list_traversal函数中的回调函数，
 * 用于针对线程队列的处理，打印任务信息 
 */
static bool elem2thread_info(struct node* pelem, int arg UNUSED) 
{
    struct task_struct* pthread = 
                container_of(struct task_struct, all_list_tag, pelem);
    char out_pad[16] = {0};

    pad_print(out_pad, 16, &pthread->pid, 'd');

    if (pthread->parent_pid == -1) 
    {
        pad_print(out_pad, 16, "NULL", 's');
    } 
    else 
    { 
        pad_print(out_pad, 16, &pthread->parent_pid, 'd');
    }

    switch (pthread->status) 
    {
        case 0:
            pad_print(out_pad, 16, "RUNNING", 's');
            break;

        case 1:
            pad_print(out_pad, 16, "READY", 's');
            break;

        case 2:
            pad_print(out_pad, 16, "BLOCKED", 's');
            break;

        case 3:
            pad_print(out_pad, 16, "WAITING", 's');
            break;

        case 4:
            pad_print(out_pad, 16, "HANGING", 's');
            break;

        case 5:
            pad_print(out_pad, 16, "DIED", 's');
    }
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    memset(out_pad, 0, 16);
    kassert(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    sys_write(stdout_no, out_pad, strlen(out_pad));

    /* 此处返回false是为了迎合主调函数list_traversal，
     * 只有回调函数返回false时才会继续调用此函数
     */
    return false;	
}


/* 打印任务列表 */
void sys_ps(void) 
{
    char* ps_title = "PID            PPID           "
                     "STAT           TICKS          COMMAND\n";
    sys_write(stdout_no, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}


/* 初始化线程环境 */
void thread_init(void)
{
    put_str("thread_init ... ");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    lock_init(&pid_lock);

    
    /* 先创建第一个用户进程: init 
     * 放在第一个初始化，这是第一个进程，init进程的pid为1
     */
    process_execute(init, "init"); 
    
    /* 将当前main函数创建为线程 */
    make_main_thread();

    /* 创建idle线程 */
    idle_thread = thread_start("idle", 10, idle, NULL);
    
    put_str("ok\n");
}
