#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* 预留给用户态程序的固定地址区，便于当前内核直接加载执行 */
#define USER_PROGRAM_BASE 0x00800000u
#define USER_PROGRAM_MAX_SIZE (1024u * 1024u)
#define USER_STACK_TOP 0x00C00000u
#define USER_STACK_SIZE 0x00001000u

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

struct multiboot_mmap_entry {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    uint32_t type;
} __attribute__((packed));

void memory_init(uint32_t mb_magic, uint32_t mb_info_addr);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t phys_addr);
void paging_mark_user_range(uint32_t phys_addr, size_t size);

void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

#endif
