; 注意：每个 ISR 都必须有自己的入口点，且要处理 EOI
global isr_common_stub
isr_common_stub:
    pushad              ; 保存所有通用寄存器
    
    ; 发送 EOI (End of Interrupt) 给主 PIC
    mov al, 0x20
    out 0x20, al        

    popad               ; 恢复所有通用寄存器
    iret                ; 这一步非常重要！跳回被中断的地方
