/* bitmap.h
 */

#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H

#include <stddef.h>
#include <stdint.h>

/* 用于按位与来判断相应的位是否为1
 * 位图中，为1的位表示资源不可用，为0表示资源可用
 */
#define BITMAP_MASK     1

/* 在遍历位图时，整体上以字节为单位，单字节上是以位为单位，
 * 因此位图的指针必须是单字节的
 */
typedef struct {
    uint32_t  len;    /* 以字节为单位的长度 */
    uint8_t * bits;
} bitmap;

void bitmap_init(bitmap *bmp);
bool bit_true(bitmap *bmp, uint32_t index);
int bitmap_alloc(bitmap *bmp, uint32_t size);
void bitmap_set(bitmap *bmp, uint32_t index, uint8_t vlaue);

#endif  /* __LIB_KERNEL_BITMAP_H */
