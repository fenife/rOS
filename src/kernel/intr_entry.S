; intr_handler.S
;   用汇编编写33个一样的中断处理程序：打印字符串

[bits 32]
; 若在相关的异常中cpu已经自动压入了错误码，为保持栈中格式统一，这里不做操作
%define ERROR_CODE  nop
; 若在相关的异常中cpu没有压入错误码,为了统一栈中格式，就手工压入一个0
%define ZERO        push 0

extern intr_handler_table   ; C中注册的中断处理程序数组
section .data

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

    ; 调用C程序会破坏当前的寄存器环境，
    ; 所以要保存上下文环境（4个段寄存器和8个通用寄存器）
    push ds
    push es
    push fs
    push gs
    pushad

    ; 如果是从片上进入的中断，除了往从片上发送EOI外，还要往主片上发送EOI
    mov al, 0x20    ; 中断结束命令EOI
    out 0xa0, al    ; 向从片发送
    out 0x20, al    ; 向主片发送

    ; 不管intr_handler_table中的目标中断处理程序是否需要参数，
    ; 都一律压入中断向量号,方便调试
    push %1

    ; 调用 intr_handler_table 中的C版本中断处理函数
    call [intr_handler_table + %1*4]
    jmp intr_exit

; 多个属性相同的.data段最终会合并到一个大的segment中，
; 最终形成intr_entry_table数组
. section .data
    dd intr_%1_entry    ; 存储各个中断入口程序的地址
%endmacro

section .text
global intr_exit
intr_exit:
    ; 中断处理程序执行完成，恢复上下文环境
    add esp, 4      ; 跳过中断号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4      ; 跳过error_code
    iretd

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
VECTOR 0x20,ZERO    ;时钟中断对应的入口
VECTOR 0x21,ZERO    ;键盘中断对应的入口
VECTOR 0x22,ZERO    ;级联用的
VECTOR 0x23,ZERO    ;串口2对应的入口
VECTOR 0x24,ZERO    ;串口1对应的入口
VECTOR 0x25,ZERO    ;并口2对应的入口
VECTOR 0x26,ZERO    ;软盘对应的入口
VECTOR 0x27,ZERO    ;并口1对应的入口
VECTOR 0x28,ZERO    ;实时时钟对应的入口
VECTOR 0x29,ZERO    ;重定向
VECTOR 0x2a,ZERO    ;保留
VECTOR 0x2b,ZERO    ;保留
VECTOR 0x2c,ZERO    ;ps/2鼠标
VECTOR 0x2d,ZERO    ;fpu浮点单元异常
VECTOR 0x2e,ZERO    ;硬盘
VECTOR 0x2f,ZERO    ;保留

;;;;;;;;;;;;;;;;   0x80号中断   ;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
    ; 1.保存上下文环境
    push 0      ; 压入中断错误码0，使栈中格式统一
    push ds
    push es
    push fs
    push gs
    pushad      ; PUSHAD指令压入32位寄存器，其入栈顺序是:
                ; EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
    push 0x80   ; 此位置压入中断向量号0x80，也是为了保持统一的栈格式

    ; 2.为系统调用子功能传入参数
    push edx    ; 系统调用中第3个参数
    push ecx    ; 系统调用中第2个参数
    push ebx    ; 系统调用中第1个参数

    ; 3.调用子功能处理函数

    ; 编译器会在栈中根据C函数声明匹配正确数量的参数
    call [syscall_table + eax * 4]

    ; 跨过上面系统调用的三个参数
    add esp, 12

    ; 4.将call调用后的返回值存入待当前内核栈中eax的位置
    mov [esp + 8*4], eax
    jmp intr_exit       ; intr_exit返回，恢复上下文
