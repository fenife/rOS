; loader.S
; 内核加载程序：
;   加载内核文件kernel.bin，初始化内核并跳转执行
;----------------------------------------------------------
%include "boot.h"
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
    add edx, 0x100000   ; 0x88子功能只会返回1MB以上的内存，
                        ; 故实际内存大小要加上1MB

.mem_get_ok:
    mov [total_mem_bytes], edx  ; 将内存换为byte单位后存入total_mem_bytes处


; ------------------ 进入保护模式 ---------------
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

; ------------------ 加载kernel ---------------
    mov eax, KERNEL_START_SECTOR    ; kernel.bin 所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR   ; 从硬盘读出后，写入到ebx指定的地址
    mov ecx, 200                    ; 读入的扇区数

    call rd_disk_m_32

; ------------------ 创建页表等 ---------------
; 创建页目录及页表并初始化页内存位图
    call setup_page

; 将描述符表地址及偏移量写入内存gdt_ptr，一会再用新地址加载
    sgdt [gdt_ptr]          ; 保存GDTR寄存器的内容到gdt_ptr所在的内存中

; 将GDT描述符中显存段描述符中的段基址 +0xc000_0000
    mov ebx,  [gdt_ptr + 2]     ; 跳过前2字节的GDT界限，得到GDT基址

    ; 显存段是第3个描述符，每个描述符占8字节，故为0x18 = 24
    ; 段描述符的高4字节的最高位是段基址的第31~24位
    or  dword [ebx + 0x18 + 4], 0xc0000000

; 将GDT的基地址加上0xc0000000使其成为内核所在的高地址
    add dword [gdt_ptr + 2], 0xc0000000

; 将栈指针同样映射到内核地址
    add esp, 0xc0000000

; 把页目录地址赋给cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

; 打开cr0的PG位
    mov eax, cr0
    or  eax, 0x80000000     ; cr0的最高位(bit 31)是PG位，置1
    mov cr0, eax

; 在开启分页后，用GDT的新地址重新加载
    lgdt [gdt_ptr]

; 为防止以后出现各种奇怪的问题，强制刷新流水线，更新gdt
    jmp SELECTOR_CODE: enter_kernel

enter_kernel:
    call kernel_init
    mov esp, 0xc009f000     ; 重新规划栈
    jmp KERNEL_ENTRY_POINT  ; 跳转执行内核


; ------------------ 初始化内核 ---------------
; 初始化内核就是根据elf规范将内核文件中的段(segment)展开到（复制到）
; 内存中的相应位置
kernel_init:
    xor eax, eax
    xor ebx, ebx    ; ebx记录程序头表地址
    xor ecx, ecx    ; cx记录程序头表中的program helader数量
    xor edx, edx    ; dx记录program header尺寸，即e_phentsize

    ; 偏移文件42字节处的属性是e_phentsize，表示program header大小
    mov dx,  [KERNEL_BIN_BASE_ADDR + 42]

    ; 偏移文件开始部分28字节的地方是e_phoff，
    ; 表示第1个program header（即第1个段）在文件中的偏移量
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]

    ; 此时ebx指向程序头表基址，第一个program header（段信息结构）
    add ebx, KERNEL_BIN_BASE_ADDR

    ; 偏移文件开始部分44字节的地方是e_phnum，表示有几个program header
    ; 即段的个数
    mov cx,  [KERNEL_BIN_BASE_ADDR + 44]

; 复制各段
.copy_each_segment:
    ; 若p_type等于PT_NULL，说明此program header未使用
    cmp byte [ebx + 0], PT_NULL
    je .PTNULL

    ; 为函数mem_cpy压入参数，参数是从右往左依次压入
    ; 函数原型类似于 memcpy(dst, src, size)

    ; program header中偏移16字节的地方是p_filesz
    ; 作为mem_cpy的第三个参数size
    push dword [ebx + 16]

    mov eax, [ebx + 4]  ;距程序头偏移量为4字节的位置是p_offset
    ; 加上kernel.bin被加载到内存的物理地址，eax为该段的物理地址
    add eax, KERNEL_BIN_BASE_ADDR
    push eax            ; 压入第二个参数：源地址

    ; program header中偏移8字节的位置是p_vaddr，
    ; 即本段被加载到内存中的起始虚拟地址
    ; 压入第一个参数：目的地址
    push dword [ebx + 8]

    call mem_cpy        ; 调用此函数完成段复制
    add esp, 12         ; 清理栈中压入的三个参数

.PTNULL:
    add ebx, edx        ; edx为program header的大小，即e_phentsize
                        ; 在此ebx指向下一个program header
    loop .copy_each_segment
    ret

; ------------------ 逐字节拷贝函数 ---------------
; 函数原型：mem_cpy(dst,src,size)
; 输入：栈中的三个参数(dst, src, size)
; 输出：无
; -----------------------------------------------
mem_cpy:
    cld         ; 将方向标志位DF置为0，让esi、edi自动增加复制数据的大小
    push ebp    ; 备份ebp
    mov ebp, esp    ;基于ebp来获得栈中的参数
    push ecx    ; rep指令用到了ecx，但ecx对于外层段的循环还有用，故先备份
    mov edi, [ebp + 8]      ; 第一个参数：dst
    mov esi, [ebp + 12]     ; 第二个参数：src
    mov ecx, [ebp + 16]     ; 第三个参数：size
    rep movsb               ; 逐字节拷贝

    ; 恢复环境
    pop ecx
    pop ebp
    ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; ------------------ 创建页目录及页表 ---------------
setup_page:

; 先把页目录占用的空间逐字节清0
    mov ecx, 4096       ; 2^10 * 4Byte = 4KB
    mov esi, 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

; 开始创建页目录项(PDE, Page Directory Entry)
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000     ; 此时eax为第一个页表的位置(0x101000)及属性
    mov ebx, eax        ; 为下面创建pte(.create_pte)做准备，ebx为基址

    ; 页目录项的属性：用户级、可读可写，US为1，表示所有特权级别都可以访问
    or  eax, PG_US_U | PG_RW_W | PG_P

    ; 下面将页目录项0和0xc00都存为第一个页表的地址，每个页表表示4MB内存，
    ; 这样0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表
    ; 为以后将地址映射为内核地址做准备

    ; 在页目录表中的第1个目录项写入第一个页表的位置(0x101000)和属性(0x7)
    mov [PAGE_DIR_TABLE_POS + 0x0], eax

    ; 虚拟地址的 3GB~4GB 是操作系统内核空间
    ; 内核空间起始目录项的值为：1024 * 3/4 = 768 = 0x300
    ; 0x300是虚拟地址的高10位，补全剩余的低20位(0)后，
    ; 实际的虚拟地址为：0xc000_0000
    ; 一个页表项占用4个字节
    ; 因此内核空间起始目录项距页目录表项0的偏移地址为：0x300 * 4 = 0xc00
    ; 即0xc00表示第768个页表占用的目录项，0xc00以上的目录项用于内核空间
    ; 页表的0xc000_0000 ~ 0xffff_ffff 共计1GB属于内核
    ; 0x0 ~ 0xbfff_ffff 共计3GB则属于用户进程
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax

    sub eax, 0x1000     ; 此时地址为0x100000，即页目录表的起始地址，属性为0x7
    ; 让最后一个目录项指向页目录表自己的地址
    mov [PAGE_DIR_TABLE_POS + 4092], eax

; 下面创建第一个页表的页表项(PTE, Pgae Table Entry)
    mov ecx, 256    ; 暂时只用到了1MB的内存
                    ; 1M低端内存/每页大小4k = 256 个页表项
    mov esi, 0

    ; 让第一个页表映射到物理内存的最低4MB
    ; edx从物理地址0x0开始算起，每次增加一个物理页大小(4k)
    mov edx, 0x0
    or edx, PG_US_U | PG_RW_W | PG_P   ; 属性为0x7，用户级、可读可写

.create_pte:
    ; ebx已经在上面赋值为0x101000，即第一个页表的地址
    mov [ebx + esi*4], edx      ; 填充页表项
                                ; 注意每个页表项占用4个字节，计算地址时要乘以4
    add edx, 4096               ; 下一个物理页的地址，连续分配
    inc esi                     ; 下一个页表项
    loop .create_pte

; 创建内核其他页表的PDE
; 即第769~1022个页目录项，第768个已在上面创建，第1023个指向页目录表自身
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000         ; 此时eax为0x102000，即第二个页表的位置
    or  eax, PG_US_U | PG_RW_W | PG_P   ; 属性为0x7，用户级、可读可写
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254            ; 范围为第769~1022共254个目录项
    mov esi, 769            ; 从第769项开始创建

.create_kernel_pde:
    mov [ebx + esi*4], eax  ; 逐项填充页目录项
    inc esi
    add eax, 0x1000         ; 下一个页表的地址
    loop .create_kernel_pde
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;----------------------------------------------------------
; 函数：rd_disk_m_32 （意为：在32位模式下读取硬盘）
; 功能：读取硬盘的n个扇区
; 参数：
;  eax = LBA 扇区号
;  ebx  = 将数据写入的内存地址
;  ecx  = 读入的扇区数
;----------------------------------------------------------
rd_disk_m_32:

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

; 至此，硬盘控制器便从指定的lba地址(eax)处，读出连续的cx个扇区，
; 下面检查硬盘状态，不忙就能把这cx个扇区的数据读出来

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
    mov [ebx], ax   
    add ebx, 2       ; 每次读入2个字节，地址要增加2
    loop .go_on_read
    ret

;;;;;;;;;; 读取硬盘结束 ;;;;;;;;;;


