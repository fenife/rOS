/* memory.h
 */

#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include <stdint.h>
#include <bitmap.h>
#include <list.h>

#define PG_SIZE     4096        /* 页的大小 */

/***************  位图基址 ********************
 * 因为0xc009f000是内核主线程栈顶，0xc009e000是内核主线程的pcb。
 * 一个页框大小的位图可表示128M内存，位图位置安排在地址0xc009a000，
 * 这样本系统最大支持4个页框的位图，即512M
 **********************************************/
#define MEM_BITMAP_BASE    0xc009a000

/* 内核所使用的堆内存空间的起始地址
 * 0xc0000000是内核从虚拟地址3G起，0x100000意指跨过低端1M内存，
 * 使虚拟地址在逻辑上连续
 */
#define K_HEAP_START    0xc0100000

#define PG_P_1      1   /* 页表项或页目录项存在属性位：表示此页内存已存在 */
#define PG_P_0      0   /* 表示此页内存不存在 */
#define PG_RW_R     0   /* R/W 属性位值，读/执行 */
#define PG_RW_W     2   /* R/W 属性位值，读/写/执行 */
#define PG_US_S     0   /* U/S 属性位值, 系统级 */
#define PG_US_U     4   /* U/S 属性位值, 用户级 */

/* 内存池标记，用于判断用哪个内存池 */
typedef enum pool_flag {
    PF_KERNEL = 1,      /* 内核内存池 */
    PF_USER   = 2       /* 用户内存池 */
} poolfg;

/* virtual memory pool
 * 虚拟内存池，用于虚拟地址的管理
 * 相对于phm_pool，少了个成员size，是因为虚拟地址空间是4GB，
 * 可以说是无限大的空间
 */
typedef struct vm_pool {
    bitmap bm;         /* 用位图管理虚拟地址池的分配情况（以页为单位） */
    uint32_t vm_start; /* 虚拟地址的起始值，以后将以这个地址开始分配内存 */
} vm_pool;

/* 内存块 */
struct mem_block {
    struct node free_elem;
};

/* 内存块描述符 */
struct mem_block_desc {
    uint32_t size;          /* 内存块大小 */
    uint32_t blocks;        /* 本arena中可容纳此mem_block的数量 */
    struct list free_list;  /* 空闲内存块链表 */
};

#define DESC_CNT    7       /* 内存块描述符个数 */

extern struct vm_pool  kvm_pool;
extern struct phm_pool kernel_pool;
extern struct phm_pool user_pool;

void mem_init(void);
uint32_t * get_pte(uint32_t vaddr);
uint32_t * get_pde(uint32_t vaddr);
void * get_kernel_pages(uint32_t pg_need);
void * malloc_page(poolfg fg, uint32_t pg_need);
void malloc_init(void);
uint32_t addr_v2p(uint32_t vaddr);
void * get_a_page(poolfg pf, uint32_t vaddr);
void * get_user_pages(uint32_t pg_cnt);
void block_desc_init(struct mem_block_desc * desc_array);
void * sys_malloc(uint32_t size);
void mfree_page(poolfg pf, void * _vaddr, uint32_t pg_cnt);
void pfree(uint32_t pg_phy_addr);

#endif  /* __KERNEL_MEMORY_H */
