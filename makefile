# 编译器和链接器设置
CC = gcc
LD = ld
AS = nasm

# 编译参数：强制生成 32 位、禁用所有标准库和浮点运算
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2

# 链接参数：输出为 32 位 ELF，禁用标准库链接
LDFLAGS = -m elf_i386 -nostdlib

# 目标文件
OBJ = boot.o kernel.o arch/serial.o arch/disk.o fs/disk_io.o arch/io.o fs/inode.o arch/ata.o arch/keyboard.o  arch/idt.o arch/gdt_flush.o arch/vga.o arch/ext2_ls.o arch/ext2_cd.o arch/ext2_lookup.o arch/ext2_cat.o arch/path.o arch/load.o arch/panic.o

all: my_kernel.bin

# 自动推导规则：%.o 由 %.c 编译而来
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# 明确的汇编规则
%.o: %.s
	$(AS) -f elf32 $< -o $@

# 3. 链接生成内核镜像
my_kernel.bin: $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -T linker.ld -o my_kernel.bin $(OBJ)

# 运行命令
run: all
	qemu-system-i386 -hda a.img -kernel my_kernel.bin -serial stdio

clean:
	rm -f *.o *.bin
