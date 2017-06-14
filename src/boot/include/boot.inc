; boot.inc
; ---------- loader 和 kernel 的配置信息 ----------

; loader 被加载到内存的地址
LOADER_BASE_ADDR  	equ 0x900

; 硬盘采用LBA扇区编址方式，MBR位于第0个扇区，
; loader位于第2个扇区（即 loader 在硬盘上的逻辑扇区地址）
LOADER_START_SECTOR	equ 0x2

; loader占用了从第2个扇区开始的多少个扇区
; MBR中加载loader时，此为待读入的扇区数
LOADER_SECTOR_NUMBER 	equ 4

KERNEL_BIN_BASE_ADDR	equ	0x70000
KERNEL_IMAGE_BASE_ADDR	equ	0x1500
KERNEL_START_SECTOR		equ 0x9

; 页目录表的起始地址
PAGE_DIR_TABLE_POS		equ	0x100000

; ------------------ GDT描述符属性 -----------------
;;;;; 形式：DESC_字段名_字段相关信息 ;;;;;
; 下面表示的是段描述符的高32位，因此 bit xxx 也是这高32位中的bit，
;   在整个描述符中表示 (bit xxx + 32)；
;   比如下面的G字段(bit 23)位于整个描述符的bit 55

; G字段(bit 23)，字段界限的单位大小，为1，表示段界限单位为4KB
DESC_G_4K	  equ 1000_0000_0000_0000_0000_0000b
; D/B字段(bit 22)，对代码段来说是D位，为1，表示指令中的有效地址及操作数是32位
DESC_D_32	  equ  100_0000_0000_0000_0000_0000b
; L字段(bit 21)，64位代码标记，此处标记为0表示32位编程
DESC_L_32	  equ   00_0000_0000_0000_0000_0000b
; AVL字段(bit 20)，CPU不用此位，暂置为0，留给操作系统用
DESC_AVL	  equ    0_0000_0000_0000_0000_0000b

; 段界限的第二部分19~16位(bit 19~16)，与段界限的第一部分组成20个二进制1，
; 第一部分在段描述符的bit 15~0 中，总共的段界限是0xFFFFF
; 平坦模型下，实际的段界限 =  2^20 * 4KB = 2^32 = 4GB
DESC_LIMIT_CODE2  equ	   1111_0000_0000_0000_0000b
DESC_LIMIT_DATA2  equ 	   DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2 equ 	    000_0000_0000_0000_0000b

; P字段(bit 15)，段是否存在，P=1表示段存在于内存中(exist)
DESC_P_EX	  equ	      	1000_0000_0000_0000b

; DPL字段(bit 14~13)，内存段的特权级，0级为最高特权级
DESC_DPL_0	  equ	        00_0_0000_0000_0000b
DESC_DPL_1	  equ 	        01_0_0000_0000_0000b
DESC_DPL_2	  equ	        10_0_0000_0000_0000b
DESC_DPL_3	  equ	        11_0_0000_0000_0000b

; S字段(bit 12)，为0表示是系统段，为1表示是普通的内存段
DESC_S_CODE	  equ 	           1_0000_0000_0000b
DESC_S_DATA	  equ 	  	   DESC_S_CODE
DESC_S_SYS	  equ	           0_0000_0000_0000b

; type字段(bit 11~8)
; x=1,c=0,r=0,a=0 代码段是可执行的、非一致性、不可读的、已访问位清0
DESC_TYPE_CODE 	  equ		1000_0000_0000b
; x=0,e=0,w=1,a=0 数据段是不可执行的、向上扩展的、可写的、已访问位清0
DESC_TYPE_DATA	  equ		0010_00000000b

; 下面分别定义了各段的高4字节（高32位）的属性值
; (0x00 << 24) 表示段基址的第 24~31 位是0x00
; 最后的0x00是段基址的第 16~23 位
; ----- 代码段 -----
DESC_CODE_HIGH4   equ	(0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L_32 \
			+ DESC_AVL + DESC_LIMIT_CODE2 + DESC_P_EX + DESC_DPL_0 		 \
			+ DESC_S_CODE + DESC_TYPE_CODE + 0x00
; ----- 数据段 -----
DESC_DATA_HIGH4   equ	(0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L_32 \
			+ DESC_AVL + DESC_LIMIT_DATA2 + DESC_P_EX + DESC_DPL_0 		 \
			+ DESC_S_DATA + DESC_TYPE_DATA + 0x00
; ----- 显存内存段 -----
; 文本模式的显存地址空间为 0xB8000 ~ 0xBFFFF，共32KB
; 其段基址为0xB8000，低4字节为0x8000，存于描述符的低4个字节处；
; 其高4字节为0x000B，存于描述符的高4字节的 bit 7~0 处，因此最后要加上0x0B
DESC_VIDEO_HIGH4  equ	(0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L_32 \
			+ DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P_EX + DESC_DPL_0 	 \
			+ DESC_S_DATA + DESC_TYPE_DATA + 0x0B

; ------------------ 段选择子属性 -----------------
; 段选择子的第 0~1 位表示请求者的当前特权级
RPL0 	equ	00b
RPL1 	equ	01b
RPL2 	equ	10b
RPL3 	equ	11b
; TI位(bit 2)，为0表示在GDT中索引描述符，为1表示在LDT中索引描述符
TI_GDT	equ	000b
TI_LDT	equ	100b

; ------------------ 页表相关属性 -----------------
PG_P		equ 1b		; 存在位(bit 0)，1表示该页存在于物理内存中
PG_RW_R		equ	00b		; 读写位(bit 1)，0表示只读
PG_RW_W		equ 10b		; 读写位(bit 1)，1表示可读可写
PG_US_S		equ 000b	; (bit 2)，0表示处于Supervisor级，用户级(3 级)程序不可访问
PG_US_U		equ	100b	; (bit 2)，1表示User级，任意级别特权的程序都可以访问
