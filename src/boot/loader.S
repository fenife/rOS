; loader.S
; 内核加载程序：
;   利用BIOS中断0x15的三个子功能获取内存容量信息
;----------------------------------------------------------
%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

; 定义保护模式下的栈，用于初始化esp
LOADER_STACK_TOP    equ    LOADER_BASE_ADDR

;----------------------------------------------------------
; 构建GDT及其内部的描述符
;----------------------------------------------------------
; 程序编译后的地址是从上到下越来越高的，因此先定义低4字节，再高4字节
; 平坦模式下，代码段、数据段和栈段：实际的段界限是4GB，段基址都是0

; 第0个描述符不可用，防止因忘记初始化选择子（此时选择子为0）
;   而索引到此描述符的情况
GDT_BASE:   dd  0x00000000      ; 低4字节
            dd  0x00000000      ; 高4字节

; 代码段描述符
CODE_DESC:  dd  0x0000FFFF
            dd  DESC_CODE_HIGH4

; 数据段和栈段（暂时让它们共用同一个段描述符）
DATA_STACK_DESC:   dd 0x0000FFFF
                   dd DESC_DATA_HIGH4

; 显存段
; 文本模式的显存地址空间为 0xB8000 ~ 0xBFFFF
; 段基址为0xB8000，低4字节为0x8000
; 高4字节为0x000B，定义在 DESC_VIDEO_HIGH4 中
; limit = (0xBFFFF - 0xB8000)/4K = 0x7
VIDEO_DESC: dd 0x80000007
            dd DESC_VIDEO_HIGH4     ; 此时DPL已改为0

GDT_SIZE    equ  $ - GDT_BASE       ; 当前GDT描述符表的大小（单位：字节）
GDT_LIMIT   equ  GDT_SIZE - 1       ; GDT界限，以字节为单位

times 60 dq 0       ; 此处预留60个描述符的空位

; 分别构建代码段、数据段(和栈段)、显存段的选择子
; 下面这一句也相当于：(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
SELECTOR_CODE   equ  (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA   equ  (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO  equ  (0x0003<<3) + TI_GDT + RPL0

; 段描述符共有4+60 = 64个，64*8 = 512 = 0x200
; total_mem_bytes用于保存内存容量，以字节为单位，此位置比较好记
; 当前偏移loader.bin文件头0x200字节，loader.bin的加载地址是0x900，
; 故total_mem_bytes内存中的地址是0xb00，将来在内核中会引用此地址
total_mem_bytes dd 0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; 以下是GDT指针，前2字节是GDT界限（以字节为单位），后4字节是GDT起始地址
gdt_ptr  dw  GDT_LIMIT
         dd  GDT_BASE

; 手工对齐：total_mem_bytes(4Byte) + gdt_ptr(6B) + ards_buf(244B)
; + ards_nr(2B)，共256 = 0x100字节
; 使 loader_start 在文件内的偏移地址为0x300 (0x200 + 0x100)
ards_buf times 244 db 0     ; 这段buf用于保存ARDS结构体
ards_nr  dw 0               ; 用于记录ARDS结构体的数量

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
loader_start:

; ----- int 15h, eax = 0000E820h, edx = 534D4150h('SMAP') 获取内存布局 -----

    xor ebx, ebx        ; 清零，第一次调用时，ebx值要为0
    mov edx, 0x534d4150 ; edx只赋值一次，循环体中不会改变
    mov di, ards_buf    ; ARDS结构缓冲区
                        ; 获取到的内存信息将写入到ES:DI所指向的内存中

.e820_mem_get_loop:     ; 循环获取每个ARDS内存范围描述结构
    mov eax, 0x0000e820 ; 执行int 0x15后，eax值变为0x534d4150，
                        ; 所以每次执行int前都要更新为子功能号
    mov ecx, 20         ; ARDS地址范围描述符结构大小为20字节
    int 0x15
    jc .e820_failed_so_try_e801     ; 若CF位为1则有错误发生，尝试0xe801子功能

    add di, cx          ; 使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr] ; 记录ARDS数量
    cmp ebx, 0          ; 若ebx为0且CF不为1，说明ARDS已全部返回，当前是最后一个
    jnz .e820_mem_get_loop  ; ebx != 0，则继续获取ARDS

; 在所有ards结构中，找出(base_addr_low + length_low)的最大值，即内存的容量
    mov cx,  [ards_nr]  ; 遍历每一个ARDS结构体，循环次数是ARDS的数量
    mov ebx, ards_buf
    xor edx, edx        ; edx中存放最大的内存容量，在此先清0

.find_max_mem_area:     ; 无须判断type是否为1，最大的内存块一定是可被使用的
    mov eax, [ebx]      ; base_addr_low
    add eax, [ebx+8]    ; length_low
    add ebx, 20         ; 指向缓冲区中下一个ARDS结构
    cmp edx, eax        ; 冒泡排序，找出最大，edx寄存器始终是最大的内存容量
    jge .next_ards      ; edx >= eax 时跳转，查找下一个ARDS
    mov edx, eax        ; edx < eax时，更新edx；edx为总内存大小

.next_ards:
    loop .find_max_mem_area     ; CX为0是退出循环
    jmp .mem_get_ok             ; 成功退出循环后跳转

; ------  int 15h, ax = E801h 获取内存大小,最大支持4G  ------
; 返回后，ax/cx中的值一样，以KB为单位，bx/dx中的值一位，以64KB为单位
; 在ax和cx寄存器中为低16M，在bx和dx寄存器中为16MB到4GB
.e820_failed_so_try_e801:
    mov ax, 0xe801
    int 0x15
    jc .e801_failed_so_try_88   ; 若当前e801方法失败，就尝试0x88方法

    ; 1.先算出低15MB的内存
    ; ax和cx中是以KB为单位的内存数量，将其转换成以byte为单位
    ; 16位乘法，被乘数是ax，积为32位，积的高16位在dx中，积的低16位在ax中
    mov cx,  0x400      ; 0x400 = 1024 = 1KB，cx用作乘数
    mul cx
    shl edx, 16         ; edx左移16位，即把dx移到高16位中
    and eax, 0x0000FFFF ; 只保留eax的低16位，即ax的值
    or  edx, eax        ; 把积的低16位组合到edx，成为32位的积
    add edx, 0x100000   ; ax只是15MB，故要加1MB
    mov esi, edx        ; 先把15MB的内存容量存入esi中备份

    ; 2.再将16MB以上的内存转换为byte为单位
    ; bx和dx中是以64KB为单位的内存数量
    xor eax, eax        ; 清零
    mov ax,  bx
    mov ecx, 0x10000   ; 0x10000 = 64KB
    ; 32位乘法，默认的被乘数是eax，积为64位
    ; 高32位存入edx，低32位存入eax
    mul ecx
    add esi, eax        ; 由于此方法只能测出4GB以内的内存，故32位eax足够了，
                        ; edx肯定为0，只加上eax即可
    mov edx, esi        ; edx为总内存大小
    jmp .mem_get_ok

; ------------ int 15h, ah = 0x88 获取内存大小,只能获取64M之内  ----------
.e801_failed_so_try_88:
    ; int 15 后，ax中存入的是以KB为单位的内存容量
    mov ah, 0x88
    int 0x15
    jc .error_hlt
    and eax, 0x0000FFFF ; 保留ax的值

    ; 16位乘法，被乘数是ax，积为32位，积的高16位在dx中，积的低16位在ax中
    mov cx,  0x400      ; 0x400 = 1KB
    mul cx
    shl edx, 16         ; 把dx移到高16位
    or  edx, eax        ; 把积的低16位组合到edx，成为32位的积
    add edx, 0x100000   ; 0x88子功能只会返回1MB以上的内存，故实际内存大小要加上1MB

.mem_get_ok:
    mov [total_mem_bytes], edx  ; 将内存换为byte单位后存入total_mem_bytes处


; ------------------ 准备进入保护模式 ---------------
; 1.打开A20
; 2.加载GDT
; 3.将cr0的PE位置1

    ; ---------- 打开A20 ----------
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

    ; ---------- 加载GDT ----------
    lgdt [gdt_ptr]

    ; ------ 将cr0的PE位置1 -------
    mov eax, cr0
    or  eax, 0x00000001
    mov cr0, eax

    ; 无条件跳转，刷新流水线，避免分支预测的影响
    ; 这将导致之前做的预测失效，从而起到刷新的作用
    jmp dword SELECTOR_CODE:p_mode_start

.error_hlt:         ; 出错则挂起
    hlt

[bits 32]
p_mode_start:
    mov ax,  SELECTOR_DATA
    mov ds,  ax
    mov es,  ax
    mov ss,  ax
    mov esp, LOADER_STACK_TOP
    mov ax,  SELECTOR_VIDEO
    mov gs,  ax

    mov byte [gs:160], 'P'  ; 在第二行行首显示字符'P'

    jmp $       ; 通过死循环使程序悬停在此
