global outb
outb:
    mov dx, [esp+4]
    mov al, [esp+8]
    out dx, al
    ret

global inb
inb:
    mov dx, [esp+4]
    in al, dx
    ret

global insw
insw:
    mov dx, [esp+4]    ; 端口
    mov edi, [esp+8]   ; 缓冲区
    mov ecx, [esp+12]  ; 总字数
    rep insw           ; 将端口数据循环存入 edi 指向的内存
    ret
    
