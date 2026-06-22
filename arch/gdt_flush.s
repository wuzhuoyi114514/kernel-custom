global gdt_flush
gdt_flush:
    mov eax, [esp+4]  ; 正确：获取 C 语言传过来的 &gp 指针
    lgdt [eax]        ; 正确：加载该指针指向的 6 字节结构体内容

    mov ax, 0x10      ; 0x10 是你的内核数据段选择子 (GDT[2])
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush   ; 0x08 是你的内核代码段选择子 (GDT[1])，远跳转刷新 CS
.flush:
    ret               ; 此时堆栈平衡，安全返回 C 语言
