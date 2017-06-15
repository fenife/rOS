; print.S
;   实现功能：打印单个字符（直接写显存）
;   暂时只处理回车、换行、退格这三种控制字符；其他的都被认为是可见字符

; 定义显存段的选择子，显存段描述符的索引值为0x0003
TI_GDT  equ 0
RPL0    equ 0
SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text
;------------------------   put_char   ----------------------------
;功能描述:把栈中的1个字符写入光标所在处
;------------------------------------------------------------------
global put_char
put_char:
    pushad      ; 备份8个32位寄存器
    ; 用段寄存器gs来存储显存段选择子，保险起见，每次打印时都为gs赋值
    mov ax, SELECTOR_VIDEO  ; 不能直接把立即数送入段寄存器
    mov gs, ax

;;;;;;;;;  获取当前光标位置 ;;;;;;;;;
    ; 先获得高8位
    mov dx, 0x03d4      ; 索引寄存器
    mov al, 0x0e        ; 用于获取光标位置高8位的索引值
    out dx, al
    mov dx, 0x03d5      ; 通过读写数据端口0x3d5来获得或设置光标位置
    in  al, dx          ; 得到了光标位置的高8位
    mov ah, al

    ; 再获取低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in  al, dx

    ; 将光标值存入bx，bx将用途基址寻址
    mov bx, ax

;;;;;;;;; 获取并分析位于栈中待打印的字符 ;;;;;;;;;
    ; pushad指令压入了4*8=32字节，加上主调函数的返回地址4字节，共36字节
    mov ecx, [esp + 36]

    ; 不管是回车还是换行，将一律按linux中的回车换行符(CRLF)处理
    cmp cl, 0xd         ; CR是0x0d，LF是0x0a
    jz .is_carriage_return
    cmp cl, 0xa
    jz .is_line_feed

    cmp cl, 0x8         ; BS(backspace)的ascii码是0x08
    jz .is_backspace

    jmp .put_other


;;;;;;;;; 处理退格符 ;;;;;;;;;
; 当为backspace时,本质上只要将光标移向前一个显存位置即可，后面再输入的字符
; 自然会覆盖此处的字符，但有可能在键入backspace后并不再键入新的字符，这时在
; 光标已经向前移动到待删除的字符位置，但字符还在原处，这就显得比较怪异，所以
; 此处添加了空格或空字符0
.is_backspace:
    dec bx      ; 光标指向前一个字符
    shl bx, 1   ; 光标左移1位相当于乘2，表示光标对应对应显存中的偏移字节

    ; 往光标所在位置的低字节处写入空格(space)的ascii码(0x20)
    mov byte [gs:bx], 0x20
    inc bx      ; bx加1指向空格字符的高字节（表示属性）
    ; 0x7表示黑屏白字，也是显卡默认的属性
    mov byte [gs:bx], 0x07
    shr bx, 1   ; bx右移1位相当于除2取整，
                ; 此时bx由显存中的相对地址恢复成了光标坐标
    jmp .set_cursor

;;;;;;;;; 处理其他可见字符 ;;;;;;;;;
.put_other:
    shl bx, 1   ;光标位置是用2字节表示,将光标值乘2,表示对应显存中的偏移字节
    mov byte [gs:bx], cl    ; ascii字符本身
    inc bx
    mov byte [gs:bx], 0x07  ; 字符属性
    shr bx, 1       ; 恢复光标坐标
    inc bx          ; 指向下一个光标值（下一个字符）

    ; 80*25模式下屏幕可显示的字符数是2000
    ; 若光标值小于2000,表示未写到显存的最后,则去设置新的光标值
    ; 若超出屏幕字符数大小(2000)则借助下面处理回车换行符的代码进行换行处理
    cmp bx, 2000
    jl .set_cursor

;;;;;;;;; 处理回车换行符 ;;;;;;;;;
.is_line_feed:          ; 换行符LF(\n)
.is_carriage_return:    ; 回车符CR(\r)
    ; 如果是CR(\r),只要把光标移到行首就可以
    ; 由于是效仿linux，linux中\n便表示是下一行的行首，所以本系统中，
    ; 把\n和\r都处理为linux中\n的意思，也就是下一行的行首

    xor dx, dx      ; dx是被除数的高16位，清0
    mov ax, bx      ; ax是被除数的低16位
    mov si, 80
    div si          ; 对80求模，余数在dx中
    sub bx, dx      ; 光标值减去除80的余数便是取整
                    ; 此时bx（即光标值）为当前行首的坐标

.is_carriage_return_end:    ; 回车符处理结束
    add bx, 80      ; 光标指向下一行的行首（此时换行成功）
    cmp bx, 2000    ; 若换行后还是超出了屏幕字符数，直接执行下面的滚屏代码
.is_line_feed_end:          ; 若是LF(\n),将光标值+80便可
    jl .set_cursor

;;;;;;;;; 屏幕向上滚动一行 ;;;;;;;;;
; 屏幕行范围是0~24，滚屏的原理是将屏幕的1~24行搬运到0~23行，
; 再将第24行用空格填充
.roll_screen:       ; 若超出屏幕大小，开始滚屏
    cld             ; 清除方向位，复制数据时让地址自动增加
    mov ecx, 960    ; 一共有2000-80=1920个字符要搬运，共1920*2=3840字节
                    ; 一次搬4字节,共3840/4=960次
    ; 文本模式的显存起始地址：0xb8000
    mov esi, 0xb80a0    ; 第1行行首：80*2 = 160 = 0xa0
    mov edi, 0xb8000    ; 第0行行首
    rep movsd           ; 循环复制

;;;;;;;;; 将最后一行填充为空白 ;;;;;;;;;
    mov ebx, 3840       ; 最后一行首字符的第一个字节偏移 = 1920 * 2
    mov ecx, 80         ; 一行是80字符(160字节)，每次清空1字符(2字节)，
                        ; 一行需要移动80次
.cls:
    mov word [gs:ebx], 0x0720   ; 0x0720是黑底白字的空格键
    add ebx, 2
    loop .cls
    mov bx, 1920        ; 将光标值重置为1920，即最后一行的首字符

;;;;;;;;; 完成光标坐标的更新 ;;;;;;;;;
; 将光标设为bx值
.set_cursor:
    ; 先设置高8位
    mov dx, 0x03d4      ; 索引寄存器
    mov al, 0x0e        ; 用于提供光标位置高8位的索引值
    out dx, al
    mov dx, 0x03d5      ; 通过读写数据端口0x3d5来获得或设置光标位置
    mov al, bh
    out dx, al          ; 设置了光标位置的高8位

    ; 再设置低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

.put_char_done:
    popad               ; 把之前入栈的8个32位寄存器恢复
    ret
