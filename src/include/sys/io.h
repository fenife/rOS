/* io.h
 * 用内联汇编实现端口I/O函数
 * 此处用内联+汇编的形式，是为了省去函数调用的开销，提高I/O速度
 */


/**************	 机器模式     *************************
	 b -- 输出寄存器QImode名称,即寄存器中的最低8位:[a-d]l。
	 w -- 输出寄存器HImode名称,即寄存器中2个字节的部分,如[a-d]x。

	 HImode
	     “Half-Integer”模式，表示一个两字节的整数。
	 QImode
	     “Quarter-Integer”模式，表示一个一字节的整数。
***************************************************/
#ifndef __LIB_KERNEL_IO_H
#define __LIB_KERNEL_IO_H

#include <stdint.h>

/* 向端口port写入一个字节 */
static inline void outb(uint16_t port, uint8_t data)
{
    /* outb的指令：outb %al, %dx
     * a 表示用寄存器al或ax或eax，
     * 对端口指定N表示0~255，d表示用dx存储端口号，
     * %b0表示对应al，%w1表示对应dx
     */
    asm volatile ("outb %b0, %w1": : "a"(data), "Nd"(port));
}

/* 将addr处起始的word_cnt个字写入端口port */
static inline void outsw(uint16_t port, const void *addr, uint32_t word_cnt)
{
    /* + 表示此限制即做输入又做输出，
     * S 表示寄存器esi/si
     * c 表示寄存器ecx/cx/cl
     * d 表示寄存器edx/dx/dl
     * outsw是把ds:esi处的16位的内容写入port端口，
     * 我们在设置段描述符时，已经将ds,es,ss段的选择子
     * 都设置为相同的值了，此时不用担心数据错乱
     */
    asm volatile ("cld; rep outsw"        \
            : "+S"(addr), "+c"(word_cnt)  \
            : "d"(port));
}

/* 将从端口port读入的一个字节返回 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;

    /* = 表示只写，相当于赋值
     * a 表示用寄存器al或ax或eax，
     * 对端口指定N表示0~255，d表示用dx存储端口号，
     * %w1表示对应dx，%b0表示对应al
     */
    asm volatile ("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

/* 将从端口port读入的word_cnt个字写入addr */
static inline void insw(uint16_t port, void *addr, uint32_t word_cnt)
{
    /* insw是将从端口port处读入的16位内容写入es:edi指向的内存，
     * 我们在设置段描述符时, 已经将ds,es,ss段的选择子都设置为相同的值了，
     * 此时不用担心数据错乱
     * + 表示此限制即做输入又做输出，
     * D 表示寄存器edi/di
     * c 表示寄存器ecx/cx/cl
     * d 表示寄存器edx/dx/dl
     * memory 告诉gcc，此汇编中修改了内存
     */
     asm volatile ("cld; rep insw" \
            : "+D"(addr), "+c"(word_cnt) \
            : "d"(port) \
            : "memory");
}

#endif  /* __LIB_KERNEL_IO_H */
