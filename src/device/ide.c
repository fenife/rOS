/* ide.c
 */

#include <ide.h>
#include <printk.h>
#include <interrupt.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <global.h>
#include <print.h>
#include <debug.h>
#include <io.h>
#include <timer.h>

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)	 (channel->port_base + 0)
#define reg_error(channel)	 (channel->port_base + 1)
#define reg_sect_cnt(channel)	 (channel->port_base + 2)
#define reg_lba_l(channel)	 (channel->port_base + 3)
#define reg_lba_m(channel)	 (channel->port_base + 4)
#define reg_lba_h(channel)	 (channel->port_base + 5)
#define reg_dev(channel)	 (channel->port_base + 6)
#define reg_status(channel)	 (channel->port_base + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

/* reg_alt_status寄存器的一些关键位 */
#define BIT_STAT_BSY	 0x80   /* 硬盘忙 */
#define BIT_STAT_DRDY	 0x40	/* 驱动器准备好 */
#define BIT_STAT_DRQ	 0x8	/* 数据传输准备好了 */

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS	0xa0	    /* 第7位和第5位固定为1 */
#define BIT_DEV_LBA	0x40
#define BIT_DEV_DEV	0x10

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY	   0xec	    /* identify指令 */
#define CMD_READ_SECTOR	   0x20     /* 读扇区指令 */
#define CMD_WRITE_SECTOR   0x30	    /* 写扇区指令 */

/* 定义可读写的最大扇区数，调试用的，只支持80MB硬盘 */
#define max_lba ((80*1024*1024 / 512) - 1)

uint8_t channel_cnt;                /* 按硬盘数计算的通道数 */
struct ide_channel channels[2];     /* 两个ide通道 */


/* 选择读写的硬盘是主盘还是从盘 */
static void select_disk(struct disk * hd)
{
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;

    /* 若是从盘就置DEV位为1 */
    if (hd->dev_no == 1)
    {
        reg_device |= BIT_DEV_DEV;
    }

    outb(reg_dev(hd->my_channel), reg_device);
}

/* 向硬盘控制器写入起始扇区地址及要读写的扇区数
 * 硬盘指针hd，扇区起始地址lba，扇区数sec_cnt
 */
static void select_sector(struct disk * hd, uint32_t lba, uint8_t sec_cnt)
{
    kassert(lba <= max_lba);

    struct ide_channel * channel = hd->my_channel;

    /* 写入要读写的扇区数
     * 如果sec_cnt为0，则表示写入256个扇区
     */
    outb(reg_sect_cnt(channel), sec_cnt);

    /* 写入lba地址(即扇区号)  */

    /*
     * lba地址的低8位，不用单独取出低8位
     * outb函数中的汇编指令outb %b0, %w1会只用al
     */
    outb(reg_lba_l(channel), lba);
    outb(reg_lba_m(channel), lba >> 8);     /* lba地址的8~15位 */
    outb(reg_lba_h(channel), lba >> 16);    /* lba地址的16~23位 */

    /* 因为lba地址的24~27位要存储在device寄存器的0～3位，
     * 无法单独写入这4位，所以在此处把device寄存器再重新写入一次
     */
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA |
            (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

/* 向通道channel发命令cmd */
static void cmd_out(struct ide_channel * channel, uint8_t cmd)
{
    /* 只要向硬盘发出了命令便将此标记置为true，
     * 硬盘中断处理程序需要根据它来判断
     * 表示通道正等待来自硬盘的中断
     */
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}

/* 硬盘读入sec_cnt个扇区的数据到buf */
static void read_from_sector(struct disk *hd, void *buf, uint8_t sec_cnt)
{
    uint32_t size_in_byte;

    if (sec_cnt == 0)
    {
        /* 因为sec_cnt是8位变量，由主调函数将其赋值时，
         * 若为256则会将最高位的1丢掉变为0
         * 表示要读256个扇区
         */
        size_in_byte = 256 * 512;
    }
    else
    {
        size_in_byte = sec_cnt * 512;
    }

    /* 以字为单位，因此要除以2 */
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/* 将buf中sec_cnt个扇区的数据写入硬盘 */
static void write2sector(struct disk *hd, void *buf, uint8_t sec_cnt)
{
    uint32_t size_in_byte;

    if (sec_cnt == 0)
    {
        /* 因为sec_cnt是8位变量，由主调函数将其赋值时，
         * 若为256则会将最高位的1丢掉变为0
         * 表示要读256个扇区
         */
        size_in_byte = 256 * 512;
    }
    else
    {
        size_in_byte = sec_cnt * 512;
    }

    /* 以字为单位，因此要除以2 */
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

/* 等待30秒
 * 返回1表示硬盘已经准备好数据，可以读出
 */
static bool busy_wait(struct disk *hd)
{
    struct ide_channel * channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;    /* 等待30000毫秒 */

    while(time_limit -= 10 >= 0)
    {
        /* BSY位如果为1，表示硬盘正忙，则休眠10毫秒 */
        if (!(inb(reg_status(channel)) & BIT_STAT_BSY))
        {
            return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        }
        else
        {
            mtime_sleep(10);    /* 休眠10毫秒 */
        }
    }

    return false;
}

/* 从硬盘读取sec_cnt个扇区到buf */
void ide_read(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt)
{
    kassert(lba <= max_lba);
    kassert(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);

    /* 1.先选择操作的硬盘 */
    select_disk(hd);

    uint32_t secs_op;           /* 每次操作的扇区数 */
    uint32_t secs_done = 0;     /* 已完成的扇区数 */

    /* 因为读写扇区数端口是8位寄存器，故每次最多只能读写255个扇区，
     * 当值为0时则表示读写256个扇区
     */
     
    while(secs_done < sec_cnt)
    {
        if ((secs_done + 256) <= sec_cnt)
        {
            secs_op = 256;
        }
        else
        {
            secs_op = sec_cnt - secs_done;
        }

        /* 2.写入待读入的扇区数和起始扇区号 */
        select_sector(hd, lba + secs_done, secs_op);

        /* 3.执行的命令写入reg_cmd寄存器
         * 准备开始读数据
         */
        cmd_out(hd->my_channel, CMD_READ_SECTOR);

        /**************** 阻塞自己的时机 ****************
         * 在硬盘已经开始工作(开始在内部读数据或写数据)后
         * 才能阻塞自己，现在硬盘已经开始忙了。
         * 硬盘是低速设备，此期间最好让出CPU，故将自己阻塞，
         * 等待硬盘完成读操作后通过中断处理程序唤醒自己
         ***********************************************/
        sema_down(&hd->my_channel->disk_done);

        /* 4.检测硬盘状态是否可读 
         * 醒来后开始执行下面代码
         */
        if (!busy_wait(hd))
        {
            /* 若失败 */
            char error[64];
            sprintf(error, "%s read sector %d failed!\n", hd->name, lba);
            PANIC(error);
        }

        /* 5.把数据从硬盘的缓冲区中读出 */
        read_from_sector(hd, (void *)((uint32_t)buf + secs_done * 512), 
                        secs_op);
        secs_done += secs_op;
    }
    lock_release(&hd->my_channel->lock);
}

/* 将buf中sec_cnt个扇区数据写入硬盘 */
void ide_write(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt)
{
    kassert(lba <= max_lba);
    kassert(sec_cnt > 0);
    lock_acquire(&hd->my_channel->lock);

    /* 1.先选择操作的硬盘 */
    select_disk(hd);

    uint32_t secs_op;           /* 每次操作的扇区数 */
    uint32_t secs_done = 0;     /* 已完成的扇区数 */

    while(secs_done < sec_cnt)
    {
        if ((secs_done + 256) <= sec_cnt)
        {
            secs_op = 256;
        }
        else
        {
            secs_op = sec_cnt - secs_done;
        }

        /* 2.写入待写入的扇区数和起始扇区号 */
        select_sector(hd, lba + secs_done, secs_op);

        /* 3.执行的命令写入reg_cmd寄存器
         * 准备开始写数据
         */
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

        
        /* 4.检测硬盘状态是否可写
         * 醒来后开始执行下面代码
         */
        if (!busy_wait(hd))
        {
            /* 若失败 */
            char error[64];
            sprintf(error, "%s write sector %d failed!\n", hd->name, lba);
            PANIC(error);
        }

        /* 5.将数据写入硬盘 */
        write2sector(hd, (void *)((uint32_t)buf + secs_done * 512), secs_op);

        /* 在硬盘响应期间阻塞自己 */
        sema_down(&hd->my_channel->disk_done);
        
        secs_done += secs_op;
    }
    /* 醒来后开始释放锁 */
    lock_release(&hd->my_channel->lock);
}

/* 硬盘中断处理程序 */
void intr_hd_handler(uint8_t irq_no)
{
    /* 此程序负责处理两个通道的中断（8259A的IRQ14和IRQ15），
     * 因此中断号要么为0x2e，要么为0x2f
     */
    kassert(irq_no == 0x2e || irq_no == 0x2f);

    /* 中断号减去0x2e，便为中断通道在channels中的索引值 */
    uint8_t ch_no = irq_no - 0x2e;
    
    struct ide_channel *channel = &channels[ch_no];
    kassert(channel->irq_no == irq_no);

    /* 不必担心此中断是否对应的是这一次的expecting_intr，
     * 每次读写硬盘时会申请锁，从而保证了同步一致性 
     */
    if (channel->expecting_intr)
    {
        channel->expecting_intr = false;

        /* 唤醒读/写硬盘的程序 */
        sema_up(&channel->disk_done);

        /* 读取状态寄存器使硬盘控制器认为此次的中断已被处理，
         * 从而硬盘可以继续执行新的读写 
         */
        inb(reg_status(channel));
    }
}

/* 硬盘数据结构初始化 */
void ide_init(void)
{
    put_str("ide_init start ...");

    uint8_t hd_cnt = *((uint8_t *)(0x475));     /* 获取硬盘的数量 */
    kassert(hd_cnt > 0);

    /* 一个ide通道上有两个硬盘，根据硬盘数量反推有几个ide通道 */
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);

    struct ide_channel * channel;
    uint8_t channel_no = 0;

    /* 处理每个通道上的硬盘 */
    while (channel_no < channel_cnt)
    {
        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);

        /* 为每个ide通道初始化端口基址及中断向量 */
        switch (channel_no)
        {
            case 0:
                /* ide0通道的起始端口号是0x1f0 */
                channel->port_base = 0x1f0;
                /* 从片8259a上倒数第二的中断引脚，温盘，
                 * 也就是ide0通道的的中断向量号
                 */
                channel->irq_no = 0x20 + 14;
                break;
            case 1:
                /* ide1通道的起始端口号是0x170 */
                channel->port_base = 0x170;
                /* 从8259A上的最后一个中断引脚，
                 * 我们用来响应ide1通道上的硬盘中断
                 */
                channel->irq_no = 0x20 + 15;
                break;
        }

        /* 未向硬盘写入指令时不期待硬盘的中断 */
        channel->expecting_intr = false;
        lock_init(&channel->lock);

        /* 初始化为0，目的是向硬盘控制器请求数据后，
         * 硬盘驱动sema_down此信号量会阻塞线程，
         * 直到硬盘完成后通过发中断，由中断处理程序将此信号量sema_up
         * 唤醒线程
         */
        sema_init(&channel->disk_done, 0);

        register_handler(channel->irq_no, intr_hd_handler);

        /* 下一个channel */
        channel_no++;
    }

    put_str("ok\n");
}
