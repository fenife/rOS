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
#include <global.h>
#include <interrupt.h>

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

/* 内存仓库arena元信息 */
struct arena {
    struct mem_block_desc * desc;   /* 此arena关联的mem_block_desc */

    /* large为ture时，cnt表示的是页框数，否则cnt表示空闲mem_block数量 */
    uint32_t cnt;
    bool large;
};

/* 内核内存块描述符数组 */
struct mem_block_desc k_block_descs[DESC_CNT];

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

    /* 若分配的地址不为空，将页框清0后返回 */
    if (vaddr != NULL)
    {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    
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
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void *)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void *)vaddr;
}


/* 为vaddr分配一个物理页，专门针对fork时虚拟地址位图无须操作的情况 */
void* get_a_page_without_opvaddrbitmap(poolfg pf, uint32_t vaddr) 
{
    struct phm_pool *mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) 
    {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    
    page_table_add((void*)vaddr, page_phyaddr); 
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
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

/* 返回arena中第idx个内存块的地址 */
static struct mem_block * arena2block(struct arena * a, uint32_t idx)
{
    return (struct mem_block *)((uint32_t)a + sizeof(struct arena) 
            + idx * a->desc->size);
}

/* 返回内存块所在的arena地址 */
static struct arena * block2arena(struct mem_block * b)
{
    return (struct arena *)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存 */
void * sys_malloc(uint32_t size)
{
    enum pool_flag pf;
    struct phm_pool * mem_pool;
    uint32_t pool_size;
    struct mem_block_desc * descs;
    struct task_struct * cur_thread = running_thread();

    /* 判断用哪个内存池 */
    if (cur_thread->pgdir == NULL)
    {
        /* 若为内核线程 */
        pf = PF_KERNEL;
        pool_size = kernel_pool.size;
        mem_pool  = &kernel_pool;
        descs = k_block_descs;
    }
    else 
    {
        /* 用户进程pcb中的pgdir会在为其分配页表时创建 */
        pf = PF_USER;
        pool_size = user_pool.size;
        mem_pool  = &user_pool;
        descs = cur_thread->u_block_desc;
    }

    /* 若申请的内存不在内存池容量范围内则直接返回NULL */
    if (!(size > 0 && size < pool_size))
    {
        return NULL;
    }

    struct arena * a;
    struct mem_block * b;
    lock_acquire(&mem_pool->lock);

    /* 超过最大内存块1024,就分配页框 */
    if (size > 1024)
    {
        uint32_t page_cnt = 
                DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);
        a = malloc_page(pf, page_cnt);

        if (a == NULL)
        {
            lock_release(&mem_pool->lock);
            return NULL;
        }

        memset(a, 0, page_cnt * PG_SIZE);   /* 将分配的内存清0 */

        /* 对于分配的大块页框，将desc置为NULL，
         * cnt置为页框数，large置为true 
         */
        a->desc = NULL;
        a->cnt  = page_cnt;
        a->large = true; 
        lock_release(&mem_pool->lock);
        
        return (void *)(a + 1); /* 跨过arena大小，把剩下的内存返回 */
    }
    /* 若申请的内存小于等于1024，可在各种规格的mem_block_desc中去适配 */
    else  
    {
        uint8_t desc_idx;

        /* 从内存块描述符中匹配合适的内存块规格 */
        for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
        {
            /* 从小到大往后找，找到后退出 */
            if (size <=  descs[desc_idx].size)
            {
                break;
            }
        }

        /* 若mem_block_desc的free_list中已经没有可用的mem_block，
         * 就创建新的arena提供mem_block
         */
        if (list_empty(&descs[desc_idx].free_list))
        {
            a = malloc_page(pf, 1);
            if (a == NULL)
            {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);

            /* 对于分配的小块内存，将desc置为相应内存块描述符，
             * cnt置为此arena可用的内存块数，large置为false
             */
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].blocks;

            uint32_t block_idx;
            intr_status old_status = intr_disable();

            /* 开始将arena拆分成内存块，并添加到内存块描述符的free_list中 */
            for (block_idx = 0; block_idx < descs[desc_idx].blocks; 
                        block_idx++)
            {
                b = arena2block(a, block_idx);
                kassert(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }    
                        
            intr_set_status(old_status);            
        }   

        /* 开始分配内存块 */
        b = container_of(struct mem_block, free_elem, \
                    list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].size);

        a = block2arena(b);     /* 获取内存块b所在的arena */
        a->cnt--;   /* 将此arena中的空闲内存块数减1 */
        lock_release(&mem_pool->lock);
        return (void *)b;
    }
}


/* 将物理地址pg_phy_addr回收到物理内存池 */
void pfree(uint32_t pg_phy_addr)
{
    struct phm_pool * mem_pool;
    uint32_t bit_idx = 0;

    if (pg_phy_addr >= user_pool.pm_start)
    {
        /* 用户物理内存池 */
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.pm_start) / PG_SIZE;
    }
    else
    {
        /* 内核物理内存池 */
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.pm_start) / PG_SIZE;
    }

    /* 将位图中该位清0 */
    bitmap_set(&mem_pool->bm, bit_idx, 0);
}

/* 去掉页表中虚拟地址vaddr的映射，只去掉vaddr对应的pte */
static void page_table_pte_remove(uint32_t vaddr)
{
    uint32_t * pte = get_pte(vaddr);
    *pte &= ~PG_P_1;    /* 将页表项pte的P位置0 */

    /* 更新快表tlb */
    asm volatile ("invlpg %0" : : "m" (vaddr) : "memory");
}

/* 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址 */
static void vaddr_remove(poolfg pf, void * _vaddr, uint32_t pg_cnt)
{
    uint32_t bit_idx_start = 0;
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t cnt = 0;

    if (pf == PF_KERNEL)
    {
        /* 内核虚拟内存池 */
        bit_idx_start = (vaddr - kvm_pool.vm_start) / PG_SIZE;
        while (cnt < pg_cnt)
        {
            bitmap_set(&kvm_pool.bm, bit_idx_start + cnt++, 0);
        }
    }
    else    
    {
        /* 用户虚拟内存池 */
        struct task_struct * cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->user_vaddr.vm_start) / PG_SIZE;
        while (cnt < pg_cnt)
        {
            bitmap_set(&cur_thread->user_vaddr.bm, bit_idx_start + cnt++, 0);
        }
    }
}

/* 释放以虚拟地址vaddr为起始的cnt个物理页框 */
void mfree_page(poolfg pf, void * _vaddr, uint32_t pg_cnt)
{
    uint32_t pg_phy_addr; 
    uint32_t vaddr = (int32_t)_vaddr;
    uint32_t page_cnt = 0;

    kassert(pg_cnt >= 1 && (vaddr % PG_SIZE == 0));
    pg_phy_addr = addr_v2p(vaddr);  /* 获取虚拟地址vaddr对应的物理地址 */

    /* 确保待释放的物理内存在低端1M+1k大小的页目录+1k大小的页表地址范围外 */
    kassert((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);

    /* 判断pg_phy_addr属于用户物理内存池还是内核物理内存池 */
    if (pg_phy_addr >= user_pool.pm_start)
    {
        /* 位于user_pool内存池 */
        vaddr -= PG_SIZE;
        while (page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            /* 确保物理地址属于用户物理内存池 */
            kassert((pg_phy_addr % PG_SIZE) == 0    \
                && pg_phy_addr >= user_pool.pm_start);

            /* 先将对应的物理页框归还到内存池 */
            pfree(pg_phy_addr);

            /* 再从页表中清除此虚拟地址所在的页表项pte */
            page_table_pte_remove(vaddr);

            page_cnt++;
        }

        /* 清空虚拟地址的位图中的相应位 */
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
    else
    {
        /* 位于kernel_pool内存池 */
        vaddr -= PG_SIZE;
        while (page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            /* 确保待释放的物理内存只属于内核物理内存池 */
            kassert((pg_phy_addr % PG_SIZE) == 0 \
                && pg_phy_addr >= kernel_pool.pm_start  \
                && pg_phy_addr < user_pool.pm_start);

            /* 先将对应的物理页框归还到内存池 */
            pfree(pg_phy_addr);

            /* 再从页表中清除此虚拟地址所在的页表项pte */
            page_table_pte_remove(vaddr);

            page_cnt++;
        }

        /* 清空虚拟地址的位图中的相应位 */
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
}

/* 回收ptr所指向的内存 */
void sys_free(void * ptr)
{
    kassert(ptr != NULL);

    if (ptr == NULL)
        return;

    poolfg pf;
    struct phm_pool * mem_pool;

    /* 判断是线程还是进程 */
    if (running_thread()->pgdir == NULL)    
    {
        /* 是线程，内核内存空间 */
        kassert((uint32_t)ptr >= K_HEAP_START);

        pf = PF_KERNEL;
        mem_pool = &kernel_pool;
    }
    else
    {
        /* 是进程，用户内存空间 */
        pf = PF_USER;
        mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);

    struct mem_block * b = ptr;
    
    /* 把mem_block转换成arena，获取元信息 */
    struct arena * a = block2arena(b);
    
    kassert(a->large == 0 || a->large == 1);

    if (a->desc == NULL && a->large == true)
    {
        /* 大于1024字节的内存 */
        mfree_page(pf, a, a->cnt);
    }
    else    /* 小于等于1024的内存块 */
    {
        /* 先将内存块回收到free_list */
        list_append(&a->desc->free_list, &b->free_elem);
        a->cnt++;
        
        /* 再判断此arena中的内存块是否都是空闲，如果是就释放此arena */
        if (a->cnt == a->desc->blocks)
        {
            uint32_t block_idx;

            /* 将此arena中的所有内存块从内存描述符的free_list中去掉 */
            for (block_idx = 0; block_idx < a->desc->blocks; block_idx++)
            {
                struct mem_block * b = arena2block(a, block_idx);
                kassert(elem_find(&a->desc->free_list, &b->free_elem));
                list_remove(&b->free_elem);
            }
            
            /* 释放此arena */ 
            mfree_page(pf, a, 1);
        }
    }

    lock_release(&mem_pool->lock);
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

/* 为malloc做准备 */
void block_desc_init(struct mem_block_desc * desc_array)
{  
    uint16_t desc_idx;
    uint16_t block_size = 16;

    /* 初始化每个mem_block_desc描述符 */
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
    {
        desc_array[desc_idx].size = block_size;

        /* 初始化arena中的内存块数量 */
        desc_array[desc_idx].blocks = 
            (PG_SIZE - sizeof(struct arena)) / block_size;

        list_init(&desc_array[desc_idx].free_list);

        /* 更新为下一个规格内存块 */
        block_size *= 2;
    }
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

    /* 初始化mem_block_desc数组descs，为malloc做准备 */
    block_desc_init(k_block_descs);
    
    put_str("mem_init done\n");
}
