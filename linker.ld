ENTRY(_start)

SECTIONS
{
    . = 1M; /* 内核加载起始地址 */

    .text : ALIGN(4K)
    {
        *(.multiboot) /* 必须放在最前端，确保引导程序能在文件前 8KB 内找到魔数 */
        *(.text)
    }

    .rodata : ALIGN(4K) { *(.rodata) }
    .data   : ALIGN(4K) { *(.data) }
    .bss    : ALIGN(4K) 
    { 
        *(COMMON) 
        *(.bss) 
    }
}
