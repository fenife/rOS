/* memory.c
 * 内存管理
 */

#include <sys/memory.h>
#include <stdint.h>
#include <sys/kernel.h>

vm_pool  kvm_pool;  /* 给内核分配虚拟内存地址 */
phm_pool kph_pool;  /* 内核物理内存池 */
phm_pool uph_pool;  /* 用户物理内存池 */

/* 根据内存容量的大小初始化物理内存池的相关结构 */
static void mem_pool_init(uint32_t all_mem)
{
    printk("   mem_pool_init statr  ... \n");

    /* 页表大小 = 1页的页目录表 + 第0和第768个页目录项指向同一个页表
     *          + 第769~1022个页目录项共指向254个页表，共256个页框
     */
    uint32_t pte_size = PG_SIZE * 256;

    /* 0x100000为低端1M内存 */
    uint32_t used_mem = pte_size + 0x100000;
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
    kph_pool.pm_start = kp_start;
    kph_pool.size = kfree_pages * PG_SIZE;
    kph_pool.bm.len = kbm_len;

    /* 初始化用户空间的物理内存池 */
    uph_pool.pm_start = up_start;
    uph_pool.size = ufree_pages * PG_SIZE;
    uph_pool.bm.len = ubm_len;

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
    kph_pool.bm.bits = (void *)MEM_BITMAP_BASE;

    /* 用户内存池的位图紧跟在内核内存池位图之后 */
    uph_pool.bm.bits = (void *)(MEM_BITMAP_BASE + kbm_len);

    /******************** 输出内存池信息 **********************/
    printk("    - kernel pool bitmap start  : 0x%x\n", 
            kph_pool.bm.bits);
    printk("    - kernel pool phy addr start: 0x%x\n", 
            kph_pool.pm_start);
    printk("    - user pool bitmap start    : 0x%x\n", 
            uph_pool.bm.bits);
    printk("    - user pool phy addr start  : 0x%x\n", 
            uph_pool.pm_start);

    /* 位图初始化：全部置0 */
    bitmap_init(&kph_pool.bm);
    bitmap_init(&uph_pool.bm);

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

    printk("   mem_pool_init done\n");
}

/* 内存管理部分初始化入口 */
void mem_init(void)
{
    printk("mem_init start ... \n");

    /* 在loader.S中，用BIOS中的三种方式获取了总的物理内存容量，
     * 其值存放在地址0xb00开始处，这里把它取出来
     */
    uint32_t mem_bytes_total = (*(uint32_t *)(0xb00));

    mem_pool_init(mem_bytes_total); /* 初始化物理内存池 */

    printk("mem_init done\n");
}