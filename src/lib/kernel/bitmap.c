/* bitmap.c
 * 位图的实现与操作
 */

#include <sys/bitmap.h>
#include <string.h>
#include <sys/debug.h>

/* 将位图bmp初始化 */
void bitmap_init(bitmap *bmp)
{
    memset(bmp->bits, 0, bmp->len);
}

/* 判断index位是否为1，若为1则返回true，否则返回false */
bool bitmap_get(bitmap *bmp, uint32_t index)
{
    uint32_t byte_idx = index / 8;    /* 向下取整用于索引数组 */
    uint32_t bit_idx  = index % 8;    /* 取余用于索引单字节内的位 */

    return (bmp->bits[byte_idx] & (BITMAP_MASK << bit_idx)) ? TRUE : FALSE;
}

/* 在位图中申请连续count个位，返回其起始位下标 */
int bitmap_alloc(bitmap *bmp, uint32_t size)
{
    uint32_t byte_idx = 0;  /* 用于记录空闲位所在的字节 */

    /* 逐字节比较,蛮力法
     * 1表示该位已分配，所以若为0xff，则表示该字节内已无空闲位，
     * 向下一字节继续找
     */
    while ((0xff == bmp->bits[byte_idx]) && (byte_idx < bmp->len))
    {
        byte_idx++;
    }

    if(byte_idx == bmp->len)    /* 若找不到空闲位 */
    {
        return -1;
    }

    /* 运行到这里，说明上面必定在此字节内找到了空闲位
     * 若在位图数组范围内的某字节内找到了空闲位，
     * 再在该字节内逐位比对，返回空闲位的索引
     */
    int bit_idx = 0;
    while ((uint8_t)(BITMAP_MASK << bit_idx) & bmp->bits[byte_idx])
    {
        bit_idx++;
    }

    /* 满足需求的起始空闲位在位图内的下标 */
    int free_bit_start = byte_idx * 8 + bit_idx;
    if ( 1 == size )    /* 如果只要求分配一个空闲位，返回当前位即可 */
    {
        return free_bit_start;
    }

    /* 剩余未判断的位的个数 */
    uint32_t bit_left = (bmp->len * 8 - free_bit_start);
    uint32_t next_bit = free_bit_start + 1;
    /* 在剩余的位中找到的空闲位的个数 */
    uint32_t free_bits = -1;

    while (bit_left-- > 0)
    {
        /* 若next_bit为0，表示是空闲位 */
        if ( !(bitmap_get(bmp, next_bit)) )
        {
            free_bits++;
        }
        else
        {
            /* 若没找到有连续的size个空位，重新计数 */
            free_bits = 0;
        }

        /* 若找到连续的size个空位，重置free_bit_start，退出循环 */
        if (free_bits == size)
        {
            free_bit_start = next_bit - size + 1;
            break;
        }
        next_bit++;
    }
    return free_bit_start;
}

/* 将位图bmp的index位转为value */
void bitmap_set(bitmap *bmp, uint32_t index, uint8_t value)
{
    kassert(0 == value || 1 == value);

    uint32_t byte_idx = index / 8;
    uint32_t bit_idx  = index % 8;

    if (value)  /* 如果value为1 */
    {
        bmp->bits[byte_idx] |= (BITMAP_MASK << bit_idx);
    }
    else
    {
        bmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_idx);
    }
}
