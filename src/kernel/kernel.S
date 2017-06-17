; kernel.S
;   用汇编编写33个一样的中断处理程序：打印字符串

[bits 32]
; 若在相关的异常中cpu已经自动压入了错误码，为保持栈中格式统一，这里不做操作
%define ERROR_CODE  nop
; 若在相关的异常中cpu没有压入错误码,为了统一栈中格式，就手工压入一个0
%define ZERO        push 0

extern put_str;
section .data
intr_str db "interrupt occur!", 0xa, 0      ; 后面两个：'\n' '\0'

global intr_entry_table
intr_entry_table:

;------------------------   VECTOR   ------------------------------
; 功能描述：定义多行的宏，2个参数
;   参数1：中断向量号
;   参数2：宏，是否压入错误码
;------------------------------------------------------------------
%macro VECTOR 2

section .text
; 每个中断处理程序都要压入中断向量号，所以一个中断类型一个中断处理程序，
; 自己知道自己的中断向量号是多少
intr_%1_entry:      ; %1为此宏中的第1个参数
    %2              ; 宏，是压入错误码
    push intr_str
    call put_str
    add esp, 4      ; 跳过参数intr_str

    ; 如果是从片上进入的中断，除了往从片上发送EOI外，还要往主片上发送EOI
    mov al, 0x20    ; 中断结束命令EOI
    out 0xa0, al    ; 向从片发送
    out 0x20, al    ; 向主片发送

    add esp, 4      ; 跨过error_code
    iret            ; 从中断返回，32位下等同指令iretd

; 多个属性相同的.data段最终会合并到一个大的segment中，
; 最终形成intr_entry_table数组
. section .data
    dd intr_%1_entry    ; 存储各个中断入口程序的地址
%endmacro

;------------------------------------------------------------------
; 用宏来定义33个(0~32)中断处理程序
; 0~19号为处理器内部固定的异常类型，20~31号为Intel保留
; 自定义的最小可用中断号是32(0x20)
;------------------------------------------------------------------
VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ERROR_CODE
VECTOR 0x09, ZERO
VECTOR 0x0a, ERROR_CODE
VECTOR 0x0b, ERROR_CODE
VECTOR 0x0c, ZERO
VECTOR 0x0d, ERROR_CODE
VECTOR 0x0e, ERROR_CODE
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ERROR_CODE
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ERROR_CODE
VECTOR 0x19, ZERO
VECTOR 0x1a, ERROR_CODE
VECTOR 0x1b, ERROR_CODE
VECTOR 0x1c, ZERO
VECTOR 0x1d, ERROR_CODE
VECTOR 0x1e, ERROR_CODE
VECTOR 0x1f, ZERO

; 自定义的最小可用中断号是32(0x20)
VECTOR 0x20, ZERO
