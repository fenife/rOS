; loader.S
; 内核加载程序：
;   暂时只让其在屏幕上显示字符串"2 LOADER"
;----------------------------------------------------------
%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

; 输出背景色为绿色，前景色为红色，并且跳动的字符串"2 LOADER"
    mov byte [gs: 0x00], '2'
    mov byte [gs: 0x01], 0xA4   ; A表示绿色背景闪烁，4表示前景色为红色
    ; 上面两句也可以写成： mov word [gs: 0x00], 0xA432

    mov byte [gs: 0x02], ' '
    mov byte [gs: 0x03], 0xA4

    mov byte [gs: 0x04], 'L'
    mov byte [gs: 0x05], 0xA4

    mov byte [gs: 0x06], 'O'
    mov byte [gs: 0x07], 0xA4

    mov byte [gs: 0x08], 'A'
    mov byte [gs: 0x09], 0xA4

    mov byte [gs: 0x0a], 'D'
    mov byte [gs: 0x0b], 0xA4

    mov byte [gs: 0x0c], 'E'
    mov byte [gs: 0x0d], 0xA4

    mov byte [gs: 0x0e], 'R'
    mov byte [gs: 0x0f], 0xA4

    jmp $       ; 通过死循环使程序悬停在此
