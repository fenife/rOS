; loader.S
; 内核加载程序：
;   进入保护模式
;----------------------------------------------------------
%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

; 定义保护模式下的栈，用于初始化esp
LOADER_STACK_TOP    equ    LOADER_BASE_ADDR

    jmp loader_start

;----------------------------------------------------------
; 构建GDT及其内部的描述符
;----------------------------------------------------------
; 程序编译后的地址是从上到下越来越高的，因此先定义低4字节，再高4字节

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

; 以下是GDT指针，前2字节是GDT界限（以字节为单位），后4字节是GDT起始地址
gdt_ptr  dw  GDT_LIMIT
         dd  GDT_BASE

loadermsg  db '2 loader in real.'


loader_start:

;------------------------------------------------------------
;INT 0x10    功能号:0x13    功能描述:打印字符串
;------------------------------------------------------------
;输入:
;  AH 子功能号=13H
;  BH = 页码
;  BL = 属性(若AL=00H或01H)
;  CX＝字符串长度
;  (DH、DL)＝坐标(行、列)
;  ES:BP＝字符串地址
;  AL＝显示输出方式
;    0——字符串中只含显示字符，其显示属性在BL中。显示后，光标位置不变
;    1——字符串中只含显示字符，其显示属性在BL中。显示后，光标位置改变
;    2——字符串中含显示字符和显示属性。显示后，光标位置不变
;    3——字符串中含显示字符和显示属性。显示后，光标位置改变
;无返回值

    mov sp, LOADER_BASE_ADDR
    mov bp, loadermsg           ; ES:BP = 字符串地址
    mov cx, 17                  ; CX = 字符串长度 = 17
    mov ax, 0x1301              ; AH = 13, AL = 01h
    mov bx, 0x001f              ; 页号为0(BH = 0)，蓝底粉红字(BL = 1Fh)
    mov dx, 0x1800              ; 坐标(最后一行的行首)
    int 0x10

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

    jmp dword SELECTOR_CODE:p_mode_start    ; 刷新流水线

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
