; 1. Multiboot 头（GRUB 等引导加载程序识别内核的依据）
section .multiboot
align 4
    dd 0x1BADB002        ; 魔数 (Magic Number)
    dd 0x41              ; 标志位 (Flags: mem info + mmap)
    dd -(0x1BADB002 + 0x41) ; 校验和 (Checksum)

section .text
global _start
extern kmain             ; 声明 C 语言的入口函数
extern run_shell         ; 声明 C 语言的 shell 入口函数

_start:
    ; 1. 关中断，防止在初始化过程中被打断
    cli                  

    ; 2. 设置栈指针 (ESP)
    ; 必须在调用任何 C 函数之前完成，否则会栈溢出或破坏内存
    mov esp, stack_top   

    ; 3. 将 multiboot magic / info 作为参数传给 C 入口
    push ebx
    push eax
    call kmain           

    ; 4. 如果 kmain 返回，进入无限循环防止 CPU 乱跑
    hlt                  
.loop:
    jmp .loop

section .bss
global stack_top
; 3. 分配内核栈空间
align 16                 ; 16 字节对齐
stack_bottom:
    resb 16384           ; 分配 16KB 栈空间
stack_top:               ; 栈顶地址
section .text

global keyboard_handler_asm
extern keyboard_handler

keyboard_handler_asm:
    pusha               ; 1. 压栈保存通用寄存器
    
    push ds             ; 2. 压栈保存原有的段寄存器
    push es
    push fs
    push gs

    mov ax, 0x10        ; 3. 加载内核数据段选择子 (0x10)
    mov ds, ax
    mov es, ax

    call keyboard_handler ; 4. 调用 C 语言的键盘处理逻辑

    pop gs              ; 5. 恢复段寄存器
    pop fs
    pop es
    pop ds
    
    popa                ; 6. 恢复通用寄存器

    ; 7. 发送 EOI（End of Interrupt）给主 PIC
    ; 这一步至关重要！不发 EOI，PIC 的 ISR 寄存器中 IRQ1 位永远不清零，
    ; 导致主 PIC 屏蔽后续所有中断（包括键盘），造成"敲键盘无响应"的 Bug。
    mov al, 0x20
    out 0x20, al

    iret                ; 8. 中断返回
    ; 确保导出符号
global syscall_handler_asm
extern c_syscall_handler

syscall_handler_asm:
    cli                 ; 关中断
    pusha               ; 压入通用寄存器 (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    
    push ds             ; 保存用户态数据段
    push es
    
    ; 切换到内核数据段，确保内核里的 C 代码能正常读写全局变量
    mov ax, 0x10        
    mov ds, ax
    mov es, ax
    
    ; 把当前的栈指针作为结构体指针参数传给 C 函数
    push esp            
    call c_syscall_handler
    add esp, 4          ; 平衡栈
    
    pop es              ; 恢复用户态段寄存器
    pop ds
    popa                ; 恢复用户态通用寄存器
    
    ; 注意：此时主 PIC 不需要发送 EOI（0x20），因为 0x80 是软中断，不占用硬件 IRQ 线路
    iret                ; 完美返回 Ring 3！

global page_fault_handler_asm
extern page_fault_handler

page_fault_handler_asm:
    cli
    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call page_fault_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa
    add esp, 4          ; 丢弃 CPU 自动压入的 error code
    iret
    global asm_switch_to_user
extern g_user_kernel_esp
extern shell_loop
global user_exit_return_stub

user_exit_return_stub:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, [g_user_kernel_esp]
    sti
    jmp shell_loop   ; 跳回 shell 主循环，不重复打印欢迎语

asm_switch_to_user:
    ; C 语言调用约定传递的参数：
    ; [esp + 4] -> entry_point
    ; [esp + 8] -> user_stack_top
    mov [g_user_kernel_esp], esp
    mov ecx, [esp + 4]  ; ecx = 用户程序入口地址
    mov edx, [esp + 8]  ; edx = 用户栈顶地址

    cli                 ; 关中断，保证压栈操作的安全

    ; 1. 刷新用户态的数据段寄存器 (0x23 是你的用户数据段选择子)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; 2. 伪造 iret 栈帧
    push 0x23           ; 用户态 SS
    push edx            ; 用户态 ESP
    push 0x202           ; EFLAGS (先设为 0x02 完全禁止中断，排除时钟中断干扰)
    push 0x1B           ; 用户态 CS (0x1B 是你的用户代码段选择子)
    push ecx            ; 用户态 EIP

    iret                ; 见证奇迹的降权切换！

; ==================== 异常处理器 ====================
; 无错误码的异常 (#DE=0, #NM=7, #DF=8)
%macro ISR_NOERR 1
global isr_%1
extern generic_exception_handler
isr_%1:
    cli
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    push %1
    call generic_exception_handler
    add esp, 8
    pop gs
    pop fs
    pop es
    pop ds
    popa
    iret
%endmacro

; 有错误码的异常 (#TS=10, #NP=11, #SS=12, #GP=13, #PF=14)
%macro ISR_ERR 1
global isr_%1
extern generic_exception_handler
isr_%1:
    cli
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push esp
    push %1
    call generic_exception_handler
    add esp, 8
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 4
    iret
%endmacro

ISR_NOERR 0    ; #DE - Divide Error
ISR_NOERR 7    ; #NM - Device Not Available
ISR_NOERR 8    ; #DF - Double Fault
ISR_ERR   10   ; #TS - Invalid TSS
ISR_ERR   11   ; #NP - Segment Not Present
ISR_ERR   12   ; #SS - Stack-Segment Fault
ISR_ERR   13   ; #GP - General Protection Fault
