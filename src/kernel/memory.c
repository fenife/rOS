/* memory.c
 * 内存管理
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <memory.h>
#include <printk.h>
#include <bitmap.h>
#include <debug.h>
#include <print.h>
#include <sync.h>

/* 获取虚拟地址的高10位，即pde索引部分 */
#define PDE_IDX(addr)   ((addr & 0xffc00000) >> 22)
/* 获取虚拟地址的中间10位，即pte索引部分 */
#define PTE_IDX(addr)   ((addr & 0x003ff000) >> 12)

/* physical memory pool
 * 物理内存池，用于管理实际物理上的内核内存池和用户内存池
 */
typedef struct phm_pool {
    bitmap bm;          /* 物理内存池的位图 */
    uint32_t pm_start;  /* 本内存池所管理物理内存的起始地址 */
    uint32_t size;      /* 本内存池字节容量 */
    struct lock lock;   /* 申请内存时互斥 */
} phm_pool;

phm_pool kernel_pool;   /* 内核物理内存池 */
phm_pool user_pool;     /* 用户物理内存池 */
vm_pool  kvm_pool;      /* 给内核分配虚拟内存地址 */

/* 在pf表示的虚拟内存池中申请pg_need个虚拟页,
 * 成功则返回虚拟页的起始地址, 失败则返回NULL
 */
static void * vaddr_get(poolfg fg, uint32_t pg_need)
{
    int vaddr_start = 0;    /* 存放分配的起始虚拟地址 */
    int bit_idx_start = -1;
    uint32_t i = 0;

    if (PF_KERNEL == fg)
    {
        bit_idx_start = bitmap_alloc(&kvm_pool.bm, pg_need);
        if (-1 == bit_idx_start)
            return NULL;

        /* 将位图中本次分配的位置为1，表示已被占用 */
        while(i < pg_need)
        {
            bitmap_set(&kvm_pool.bm, bit_idx_start + i++, 1);
        }

        vaddr_start = kvm_pool.vm_start + bit_idx_start * PG_SIZE;
    }
    else    /* 用户内存池 */
    {
        struct task_struct * cur = running_thread();
        bit_idx_start = bitmap_alloc(&cur->user_vaddr.bm, pg_need);
        if (bit_idx_start == -1)
        {
            return NULL;
        }

        while (i < pg_need)
        {
            bitmap_set(&cur->user_vaddr.bm, bit_idx_start + i++, 1);
        }

        vaddr_start = cur->user_vaddr.vm_start + bit_idx_start * PG_SIZE;

        /* (0xc0000000-PG_SIZE)作为用户3级栈已经在start_process被分配 */
        kassert((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }

    return (void *)vaddr_start;
}

/* 获取虚拟地址vaddr对应的pte指针 */
uint32_t * get_pte(uint32_t vaddr)
{
    /* 先访问到页目录表自己 + \
     * 再用页目录项pde(页目录内页表的索引)做为pte的索引访问到页表 + \
     * 再用pte的索引做为页内偏移
     */
    uint32_t * pte = (uint32_t *)( 0xffc00000
            + ((vaddr & 0xffc00000) >> 10)
            + PTE_IDX(vaddr) * 4 );

    return pte;
}

/* 获取虚拟地址vaddr对应的pde指针 */
uint32_t * get_pde(uint32_t vaddr)
{
    uint32_t * pde = (uint32_t *)( (0xfffff000) + PDE_IDX(vaddr) * 4 );

    return pde;
}

/* 在pool指向的物理内存池中分配1个物理页，
 * 成功则返回页框的物理地址,失败则返回NULL
 */
static void * palloc(phm_pool *pool)
{
    /* 扫描或设置位图要保证原子操作 */
    int bit_idx = bitmap_alloc(&pool->bm, 1);   /* 分配一个物理页面 */
    if (-1 == bit_idx)
        return NULL;

    bitmap_set(&pool->bm, bit_idx, 1);

    uint32_t page_phyaddr = (pool->pm_start + (bit_idx * PG_SIZE));
    return (void *)page_phyaddr;
}

/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */
static void page_table_add(void *_vaddr, void *_page_phyaddr)
{
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t paddr = (uint32_t)_page_phyaddr;

    uint32_t * pde = get_pde(vaddr);
    uint32_t * pte = get_pte(vaddr);

    /************************ 注意   *************************
     * 执行*pte，会访问到空的pde。所以要确保pde创建完成后才能
     * 执行*pte，否则会引发page_fault。
     * 因此在*pde为0时，*pte只能出现在下面else语句块中的*pde后面
     *******************************************************/

    /* 页目录项和页表项的第0位为P，此处判断目录项是否存在
     * 若为1,则表示该表已存在
     */
    if (*pde & 0x00000001)
    {
        if (!(*pte & 0x00000001))  /* 页表项不存在，创建页表项 */
        {

			*pte = (paddr | PG_US_U | PG_RW_W | PG_P_1);
        }
        else  /* 页表已存在 */
        {
			PANIC("pte repeat\n");
            *pte = (paddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    }
    /* 页目录项不存在，所以要先创建页目录项再创建页表项 */
    else
    {
        /* 页表中用到的页框一律从内核空间分配 */
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

        /******************* 将页表所在的页清0 *********************
         * 必须把分配到的物理页地址pde_phyaddr对应的物理内存清0，
         * 避免里面的陈旧数据变成了页表中的页表项，从而让页表混乱。
         * pte的高20位会映射到pde所指向的页表的物理起始地址。
         **********************************************************/
        memset((void *)((int)pte & 0xfffff000), 0, PG_SIZE);
        kassert((*pte & 0x00000001) == 0);
        *pte = (paddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

/* 分配pg_need个物理页空间
 * 成功则返回起始虚拟地址，失败时返回NULL
 *
 * 此函数的原理是三个动作的合成：
 * 1.通过vaddr_get在虚拟内存池中申请虚拟地址空间
 * 2.通过palloc在物理内存池中申请物理页
 * 3.通过page_table_add将以上两步得到的虚拟地址和物理地址
 *   在页表中完成映射
 */
void * malloc_page(poolfg fg, uint32_t pg_need)
{
    /* 确保所需内存页数不超过物理内存的容量
     * 内存总容量为32MB
     * 内核和用户空间各约16MB，保守起见按15MB计算
     * pg_need < 15 * 1024 * 1024 / 4096 = 3840页
     */
    kassert(pg_need > 0 && pg_need < 3840);

    void * vaddr_start = vaddr_get(fg, pg_need);
    if (NULL == vaddr_start)
    {
        return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start;
    uint32_t i = pg_need;
    phm_pool * pool = fg & PF_KERNEL ? &kernel_pool : &user_pool;

    /* 因为虚拟地址是连续的，但物理地址可以是不连续的，
     * 所以要逐页做映射
     */
    while (i-- > 0)
    {
        void *page_phyaddr = palloc(pool);
        if (NULL == page_phyaddr)
        {
            /* 失败时要将曾经已申请的虚拟地址和物理页全部回滚，
             * 在将来完成内存回收时再补充
             */
            return NULL;
        }

        /* 在页表中做映射 */
        page_table_add((void *)vaddr, page_phyaddr);
        vaddr += PG_SIZE;   /* 下一个虚拟页 */
    }
    return vaddr_start;
}

/* 从内核物理内存池中申请pg_need页内存，
 * 成功则返回其虚拟地址，失败则返回NULL
 */
void * get_kernel_pages(uint32_t pg_need)
{
    lock_acquire(&kernel_pool.lock);
    void * vaddr = malloc_page(PF_KERNEL, pg_need);
    if (NULL == vaddr)
        return NULL;

    /* 若分配的地址不为空，将页框清0后返回 */
    memset (vaddr, 0, pg_need * PG_SIZE);

    lock_release(&kernel_pool.lock);
    return vaddr;
}

/* 在用户空间中申请4k内存，并返回其虚拟地址 */
void *get_user_pages(uint32_t pg_cnt)
{
    lock_acquire(&user_pool.lock);
    void * vaddr = malloc_page(PF_USER, pg_cnt);
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

/* 申请一页内存，并用vaddr映射到该页，即可以指定虚拟地址 */
void * get_a_page(poolfg pf, uint32_t vaddr)
{
    struct phm_pool* mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);

    /* 先将虚拟地址对应的位图置1 */
    struct task_struct * cur = running_thread();
    int32_t bit_idx = -1;

    /* 若当前是用户进程申请用户内存，就修改用户进程自己的虚拟地址位图 */
    if (cur->pgdir != NULL && pf == PF_USER)
    {
        bit_idx = (vaddr - cur->user_vaddr.vm_start) / PG_SIZE;
        kassert(bit_idx > 0);
        bitmap_set(&cur->user_vaddr.bm, bit_idx, 1);
    }
    else if (cur->pgdir == NULL && pf == PF_KERNEL)
    {
        /* 如果是内核线程申请内核内存，就修改kernel_vaddr */
        bit_idx = (vaddr - kvm_pool.vm_start) / PG_SIZE;
        kassert(bit_idx > 0);
        bitmap_set(&kvm_pool.bm, bit_idx, 1);
    }
    else
    {
        PANIC("get_a_page: not allow kernel userspace"
                    "or user alloc kernelspace by get_a_page");
    }

    void * page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL)
    {
        return NULL;
    }
    page_table_add((void *)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void *)vaddr;
}

/* 返回虚拟地址映射到的物理地址 */
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t * pte = get_pte(vaddr);

    /* (*pte)的值是页表所在的物理页框地址,
     * 去掉其低12位的页表项属性+虚拟地址vaddr的低12位
     */
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/* 根据内存容量的大小初始化物理内存池的相关结构 */
static void mem_pool_init(uint32_t all_mem)
{
    put_str("   mem_pool_init statr  ... \n");

    /* 页表大小 = 1页的页目录表 + 第0和第768个页目录项指向同一个页表
     *          + 第769~1022个页目录项共指向254个页表，共256个页框
     */
    uint32_t page_table_size = PG_SIZE * 256;

    /* 0x100000为低端1M内存 */
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;

    /* 1页为4k，不管总内存是不是4k的倍数，
     * 对于以页为单位的内存分配策略，不足1页的内存暂不考虑
     */
    uint16_t all_free_pages = free_mem / PG_SIZE;
    /* 分配给内核空间的空闲物理页 */
    uint16_t kfree_pages = all_free_pages / 2;
    /* 分配给用户空间的空闲物理页 */
    uint16_t ufree_pages = all_free_pages - kfree_pages;

    /* 为简化位图操作，余数不处理，坏处是这样做会丢内存；
     * 好处是不用做内存的越界检查，因为位图表示的内存少于实际物理内存
     */

    /* Kernel BitMap的长度，位图中的一位表示一页，以字节为单位 */
    uint32_t kbm_len = kfree_pages / 8;

    /* User BitMap的长度 */
    uint32_t ubm_len = ufree_pages / 8;

    /* Kernel Pool start，内核内存池的起始地址 */
    uint32_t kp_start = used_mem;

    /* User Pool start，用户内存池的起始地址 */
    uint32_t up_start = kp_start + kfree_pages * PG_SIZE;

    /* 初始化内核空间的物理内存池 */
    kernel_pool.pm_start = kp_start;
    kernel_pool.size = kfree_pages * PG_SIZE;
    kernel_pool.bm.len = kbm_len;

    /* 初始化用户空间的物理内存池 */
    user_pool.pm_start = up_start;
    user_pool.size = ufree_pages * PG_SIZE;
    user_pool.bm.len = ubm_len;

    /********* 内核内存池和用户内存池位图 ***********
     * 不用数组而用堆内存来存储位图的原因：
     * 位图是全局的数据，长度不固定。
     * 全局或静态的数组需要在编译时知道其长度；
     * 而我们需要根据总内存大小算出需要多少字节，
     * 所以改为指定一块内存来生成位图。
     *************************************************/

    /* 内核使用的最高地址是0xc009f000，这是主线程的栈地址
     * 内核的大小预计为70K左右。
     * 32M内存占用的位图是1k，内核内存池的位图先定在(0xc009a000)处
     */
    kernel_pool.bm.bits = (void *)MEM_BITMAP_BASE;

    /* 用户内存池的位图紧跟在内核内存池位图之后 */
    user_pool.bm.bits = (void *)(MEM_BITMAP_BASE + kbm_len);

    /******************** 输出内存池信息 **********************/
    printk("    - kernel pool bitmap start  : 0x%x\n",
            kernel_pool.bm.bits);
    printk("    - kernel pool phy addr start: 0x%x\n",
            kernel_pool.pm_start);
    printk("    - user pool bitmap start    : 0x%x\n",
            user_pool.bm.bits);
    printk("    - user pool phy addr start  : 0x%x\n",
            user_pool.pm_start);

    /* 位图初始化：全部置0 */
    bitmap_init(&kernel_pool.bm);
    bitmap_init(&user_pool.bm);

    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    
    /* 下面初始化内核虚拟地址的位图，按实际物理内存大小生成数组
     * 用于维护内核堆的虚拟地址，所以要和内核内存池大小一致
     */
    kvm_pool.bm.len = kbm_len;

    /* 位图的数组指向一块未使用的内存，
     * 目前定位在内核物理内存池和用户物理内存池之外
     */
    kvm_pool.bm.bits = (void *)(MEM_BITMAP_BASE + kbm_len + ubm_len);

    /* 虚拟内存池的起始地址 */
    kvm_pool.vm_start = K_HEAP_START;

    bitmap_init(&kvm_pool.bm);

    put_str("   mem_pool_init done\n");
}

/* 内存管理部分初始化入口 */
void mem_init(void)
{
    put_str("mem_init start ... \n");

    /* 在loader.S中，用BIOS中的三种方式获取了总的物理内存容量，
     * 其值存放在地址0xb00开始处，这里把它取出来
     */
    uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));

    mem_pool_init(mem_bytes_total); /* 初始化物理内存池 */

    put_str("mem_init done\n");
}
