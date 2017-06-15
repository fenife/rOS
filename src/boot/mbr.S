; mbr.S
; 主引导程序：
; 让MBR可以读取硬盘
;----------------------------------------------------------
%include "boot.h"
SECTION MBR vstart=0x7c00
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00
    mov ax, 0xb800      ; 显存文本模式的起始地址
    mov gs, ax          ; 段基址

; 清屏，利用0x06号功能，上卷全部的行，则可清屏
;----------------------------------------------------------
; INT 0x10  功能号：0x06     功能描述：上卷窗口
;----------------------------------------------------------
;输入：
; AH = 0x06 （功能号）
; AL = 上卷的行数（如果为0，表示全部）
; BH = 上卷的行属性
; (CL, CH) = 窗口左上角的(X, Y)位置
; (DL, DH) = 窗口右下角的(X, Y)位置
; 无返回值：
    mov ax, 0600h
    mov bx, 0700h
    mov cx, 0           ; 左上角：(0, 0)
    mov dx, 184fh       ; 右下角：(80, 25),
            ; VGA文本模式中，一行只能容纳80个字符，共25行
            ; 下标从0开始，所以0x18=24,0x4f=79
    int 0x10

; 输出背景色为绿色，前景色为红色，并且跳动的字符串"1 MBR"
    mov byte [gs: 0x00], '1'
    mov byte [gs: 0x01], 0xA4   ; A表示绿色背景闪烁，4表示前景色为红色
    ; 上面两句也可以写成： mov word [gs: 0x00], 0xA431

    mov byte [gs: 0x02], ' '
    mov byte [gs: 0x03], 0xA4

    mov byte [gs: 0x04], 'M'
    mov byte [gs: 0x05], 0xA4

    mov byte [gs: 0x06], 'B'
    mov byte [gs: 0x07], 0xA4

    mov byte [gs: 0x08], 'R'
    mov byte [gs: 0x09], 0xA4

    ; 下面三行为函数 rd_disk_m_16 传递参数
    mov eax, LOADER_START_SECTOR    ; 起始扇区LBA地址（28位，故用eax）
    mov bx,  LOADER_BASE_ADDR       ; 写入的地址
    mov cx,  LOADER_SECTOR_NUMBER   ; 待读入的扇区数
    call rd_disk_m_16               ; 以下读取程序的起始部分（一个扇区）

    ; 0x300 是标签 loader_start 在loader.S中的偏移地址（手工凑的）
    jmp LOADER_BASE_ADDR + 0x300    ; 跳转执行loader


;----------------------------------------------------------
; 函数：rd_disk_m_16 （意为：在16位模式下读硬盘）
; 功能：读取硬盘的n个扇区
; 参数：
;  eax = LBA 扇区号
;  bx  = 将数据写入的内存地址
;  cx  = 读入的扇区数
;----------------------------------------------------------
rd_disk_m_16:

    mov esi, eax    ; 备份eax
    mov di,  cx     ; 备份cx

;;;;;;;;;; 读取硬盘 ;;;;;;;;;;
; 第1步：设置要读取的扇区数
    ; bochs 中设置的虚拟硬盘为 ata0 ，是Primary通道，故用 0x1f2 端口
    mov dx, 0x1f2   ; 要设置的端口（寄存器）： Sector count
    mov al, cl      ; 此寄存器为8位，故取cx的低8位，即cl
    out dx, al      ; 读取的扇区数

    mov eax, esi    ; 恢复ax

; 第2步：将LBA地址存入0x1f3 ~ 0x1f6 这几个寄存器（端口）中

    ; LBA地址 7~0 bit 写入端口 0x1f3 (LBA low)
    mov dx, 0x1f3
    out dx, al

    ; LBA地址 15~8 bit 写入端口 0x1f4 (LBA mid)
    mov cl, 8
    shr eax, cl     ; 右移8位
    mov dx, 0x1f4
    out dx, al

    ; LBA地址 23~16 bit 写入端口 0x1f5 (LBA high)
    shr eax, cl
    mov dx, 0x1f5
    out dx, al

    ; LBA地址 27~24 bit 写入端口 0x1f6 (Device)的低4位
    ; 端口 0x1f6 的高4位用于设置其他属性
    shr eax, cl
    and al, 0x0f    ; LBA地址的第 24 ~ 27 位
    or  al, 0xe0    ; 设置第 7 ~ 4 bit为1110，表示LBA模式
    mov dx, 0x1f6
    out dx, al

; 第3步：向端口 0x1f7 (Status) 写入读命令：0x20
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

; 第4步：检测硬盘状态
.not_ready:
    ; 同一端口，写时表示写入命令字，读时表示读入硬盘状态
    nop             ; 空操作，增加延迟
    in al, dx
    and al, 0x88    ; 第3位为1表示硬盘控制器已准备好数据传输，
                    ; 第7位为1表示硬盘忙
    cmp al, 0x08    ; 判断第3位是否为1
    jnz .not_ready  ; 若未准备好，继续等

; 第5步：从端口 0x1f0 (Data, 16bit) 读取数据

    ; di 为要读取的扇区数，一个扇区有512字节，每次读入一个字（2字节），
    ; 共需 di*512/2 次，所以 di*256

    mov ax, di
    mov dx, 256     ; 256 超过了8位的表示范围，故用16位乘法，积为32位
    mul dx          ; ax*dx，积(32bit)的高16位存放在dx中，低16位在ax中
    mov cx, ax      ; 这里的结果不大，故只取积的低16位作为循环次数
    mov dx, 0x1f0

.go_on_read:
    in ax, dx
    mov [bx], ax    ; 实模式下，bx为16位的偏移地址，
                    ; 加载的内容不能超过64KB (2^16)
    add bx, 2       ; 每次读入2个字节，地址要增加2
    loop .go_on_read
    ret

;;;;;;;;;; 读取硬盘结束 ;;;;;;;;;;

    times 510-($-$$) db 0
    db 0x55, 0xaa
