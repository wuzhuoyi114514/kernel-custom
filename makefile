# 编译器和链接器设置
CC = gcc
LD = ld
AS = nasm

# 编译参数：强制生成 32 位、禁用所有标准库和浮点运算
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -Iinclude

# 链接参数：输出为 32 位 ELF，禁用标准库链接
LDFLAGS = -m elf_i386 -nostdlib

BUILD_DIR = build

# 目标文件
OBJ = $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/kernel/task.o $(BUILD_DIR)/arch/ata.o $(BUILD_DIR)/arch/disk.o $(BUILD_DIR)/arch/elf.o $(BUILD_DIR)/arch/ext2_cat.o $(BUILD_DIR)/arch/ext2_cd.o $(BUILD_DIR)/arch/ext2_lookup.o $(BUILD_DIR)/arch/ext2_ls.o $(BUILD_DIR)/arch/gdt_flush.o $(BUILD_DIR)/arch/idt.o $(BUILD_DIR)/arch/io.o $(BUILD_DIR)/arch/keyboard.o $(BUILD_DIR)/arch/load.o $(BUILD_DIR)/arch/memory.o $(BUILD_DIR)/arch/panic.o $(BUILD_DIR)/arch/path.o $(BUILD_DIR)/arch/serial.o $(BUILD_DIR)/arch/string.o $(BUILD_DIR)/arch/vga.o $(BUILD_DIR)/fs/disk_io.o $(BUILD_DIR)/fs/inode.o $(BUILD_DIR)/fs/vfs.o $(BUILD_DIR)/fs/exec_elf.o $(BUILD_DIR)/kernel/auto_user.o

$(BUILD_DIR)/kernel/task.o: src/kernel/task.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

all: $(BUILD_DIR)/my_kernel.bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/arch $(BUILD_DIR)/fs $(BUILD_DIR)/kernel

# 自动推导规则：build/%.o 由 %.c 编译而来
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.o: src/kernel/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/auto_user.o: src/kernel/auto_user.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


# 明确的汇编规则
$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -f elf32 $< -o $@

# 3. 链接生成内核镜像
$(BUILD_DIR)/my_kernel.bin: $(OBJ) linker.ld
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(OBJ)

# 构建用户程序
USER_CC = gcc
USER_CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -static -fno-stack-protector -fno-asynchronous-unwind-tables -Iinclude -Wl,-n -Wl,--build-id=none -Wl,-Ttext=0x800000 -Wl,-z,norelro
USER_DIR = user

$(USER_DIR)/%.elf: $(USER_DIR)/%.c
	$(USER_CC) $(USER_CFLAGS) -e _start -o $@ $<
	strip --strip-all $@

user: $(USER_DIR)/hello.elf $(USER_DIR)/shell.elf

user-install: user
	debugfs -w a.img -R "rm hello.elf" 2>/dev/null; debugfs -w a.img -R "write $(USER_DIR)/hello.elf hello.elf"
	debugfs -w a.img -R "rm shell.elf" 2>/dev/null; debugfs -w a.img -R "write $(USER_DIR)/shell.elf shell.elf"

# 运行命令
# 可通过 make run init=./hello.elf 指定 init 程序
init ?= ./shell.elf
run: all
	qemu-system-i386 -hda a.img -kernel $(BUILD_DIR)/my_kernel.bin -append "init=$(init)" -serial stdio -d int,mmu -D qemu_debug.log

user-clean:
	rm -f $(USER_DIR)/*.elf
	debugfs -w a.img -R "rm hello.elf" 2>/dev/null; true
	debugfs -w a.img -R "rm shell.elf" 2>/dev/null; true

clean:
	rm -rf $(BUILD_DIR) *.o *.bin arch/*.o fs/*.o
