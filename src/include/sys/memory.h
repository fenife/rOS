/* memory.h
 */

#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include <stdint.h>
#include <sys/bitmap.h>

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


/* virtual memory pool 
 * 虚拟内存池，用于虚拟地址的管理
 * 相对于phm_pool，少了个成员size，是因为虚拟地址空间是4GB，
 * 可以说是无限大的空间
 */
typedef struct vm_pool {
    bitmap bm;         /* 用位图管理虚拟地址池的分配情况（以页为单位） */
    uint32_t vm_start; /* 虚拟地址的起始值，以后将以这个地址开始分配内存 */
} vm_pool;


/* physical memory pool 
 * 物理内存池，用于管理实际物理上的内核内存池和用户内存池
 */
typedef struct phm_pool {
    bitmap bm;          /* 物理内存池的位图 */
    uint32_t pm_start; /* 本内存池所管理物理内存的起始地址 */
    uint32_t size;      /* 本内存池字节容量 */
} phm_pool;

extern vm_pool  kvm_pool;  
extern phm_pool kph_pool; 
extern phm_pool uph_pool;

void mem_init(void);

#endif  /* __KERNEL_MEMORY_H */
