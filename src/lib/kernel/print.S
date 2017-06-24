; print.S
;   实现整数打印，打印的数字为16进制形式
;   原理：将数字转换成对应的字符，比如数字9转换为字符'9'(0x39)

; 定义显存段的选择子，显存段描述符的索引值为0x0003
TI_GDT  equ 0
RPL0    equ 0
SELECTOR_VIDEO  equ (0x0003<<3) + TI_GDT + RPL0

section .data
put_int_buf     dq  0   ;定义8字节缓冲区用于存储转换后的字符

[bits 32]
section .text
;------------------------   put_str   ----------------------------
; 功能描述：通过put_char来打印以0字符结尾的字符串
; 输入：栈中的参数为要打印字符串的起始地址
; 输出：无
;------------------------------------------------------------------
global put_str
put_str:
    ; 由于本函数只用到了ebx和ecx，只备份这两个寄存器
    push ebx
    push ecx
    xor  ecx, ecx           ; 清0，用ecx存储要打印的字符
    ; ebx，ecx，put_str的返回地址，3*4 = 12字节
    mov  ebx, [esp + 12]    ; 从栈中获取待打印的字符串地址
.go_on:
    mov  cl, [ebx]          ; 得到要打印的字符
    cmp  cl, 0              ; 如果处理到了字符串尾，跳到结束处返回
    jz   .str_over
    push ecx                ; 为put_char函数传递参数
    call put_char
    add  esp, 4             ; 回收调用put_char时的参数所占的栈空间
    inc  ebx                ; 使ebx指向下一个字符
    jmp .go_on
.str_over:
    pop ecx
    pop ebx
    ret

;------------------------   put_char   ----------------------------
; 功能描述：把栈中的1个字符写入光标所在处
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
    mov esi, 0xc00b80a0     ; 第1行行首：80*2 = 160 = 0xa0
    mov edi, 0xc00b8000     ; 第0行行首
    rep movsd               ; 循环复制

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

;------------------------   put_int   ----------------------------
; 功能描述：实现整数打印，打印的数字为16进制形式
; 输入：栈中的参数为待打印的数字
; 输出：在屏幕上打印16进制数字，并不会打印前缀0x，
;       如打印10进制15时，只会直接打印f，不会是0xf
;------------------------------------------------------------------
global put_int
put_int:
    pushad
    mov ebp, esp
    mov eax, [ebp + 9*4]    ; call的返回地址占4字节 + pushad的8个4字节
    mov edx, eax            ; edx和eax中都是参数（要打印的数字）
    mov edi, 7              ; 指向缓冲区最后一个字节
                            ; 存储数字最低4位二进制对应的字符
    mov ecx, 8              ; 32位数字，每4位用一个16进制数字表示，共8个
    mov ebx, put_int_buf    ; 缓冲区基址

; 将32位数字按照16进制的形式从低位到高位逐个处理，共处理8个16进制数字
.16based_4bits:         ; 每4位二进制是16进制数字的1位，遍历每一位16进制数字
    and edx, 0x0000000F ; 解析16进制数字的每一位
                        ; and与操作后，edx只有低4位有效
    cmp edx, 9          ; 数字0~9和a~f需要分别处理成对应的字符
    jg .is_A2F
    add edx, '0'        ; ascii码是8位大小，add求和操作后，edx低8位有效
    jmp .store
.is_A2F:
    sub edx, 10         ; A~F 减去10所得到的差，
    add edx, 'A'        ; 再加上字符A的ascii码，便是A~F对应的ascii码
; 将每一位数字转换成对应的字符后，按照类似“大端”的顺序存储到缓冲区put_int_buf
; 高位字符放在低地址，低位字符要放在高地址，
; 这样便和大端字节序类似，只不过咱们这里是字符序
.store:
    ; 此时dl中应该是数字对应的字符的ascii码
    mov [ebx + edi], dl ; ebx为buf基址，edi为buf偏移量
    dec edi
    shr eax, 4          ; 处理数字的下一个4位
    mov edx, eax
    loop .16based_4bits

; 现在put_int_buf中已全是转换后的字符，打印前，
; 先把高位连续的字符去掉，比如把字符000123变成123
.ready_to_print:
    inc edi             ; 此时edi退减为-1(0xffffffff)，加1使其为0
.skip_prefix_0:
    cmp edi, 8          ; 若已经比较第9个字符了，表示待打印的字符串为全0
    je .full_0
; 找出连续的'0'字符，edi作为非'0'字符的最高位字符的偏移
.go_on_skip:
    mov cl, [put_int_buf + edi]     ; 取buf中非'0'的字符
    inc edi
    cmp cl, '0'
    je .skip_prefix_0   ; 继续判断下一位字符是否为字符'0'(不是数字0)
    dec edi             ; edi在上面的inc操作中指向了下一个字符，
                        ; 若当前字符不为'0'，要恢复edi指向当前字符
    jmp .put_each_num

.full_0:
    mov cl, '0'         ; 输入的数字为全0时，则只打印'0'
.put_each_num:
    push ecx            ; 此时cl中为可打印的字符，
                        ; 压入栈中作为put_char的参数
    call put_char
    add  esp, 4         ; 清理栈
    inc  edi            ; 使edi指向下一个打印的字符
    mov  cl, [put_int_buf + edi]    ; 获取下一个字符到cl
    cmp  edi, 8
    jl  .put_each_num
    popad
    ret

global set_cursor
set_cursor:
    pushad
    mov bx, [esp + 36]

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

    popad
    ret
    