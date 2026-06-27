#include "../include/memory.h"
#include "../include/debug.h"
#include "../include/vga.h"

#define PAGE_SIZE              4096u
#define PAGE_PRESENT           0x001u
#define PAGE_WRITE             0x002u
#define PAGE_USER              0x004u

#define MAX_PHYS_MEM_BYTES     (512u * 1024u * 1024u)
#define MAX_FRAMES             (MAX_PHYS_MEM_BYTES / PAGE_SIZE)
#define FRAME_BITMAP_BYTES     (MAX_FRAMES / 8u)

#define IDENTITY_MAP_BYTES     (128u * 1024u * 1024u)
#define IDENTITY_TABLES        (IDENTITY_MAP_BYTES / (4u * 1024u * 1024u))

#define HEAP_SIZE              (512u * 1024u)
#define HEAP_ALIGN             16u
#define BLOCK_MAGIC            0xC0FFEE42u

extern char kernel_end;
extern void panic(const char *msg);

typedef struct heap_block {
    uint32_t magic;
    uint32_t size;
    uint8_t free;
    uint8_t _pad[3];
    struct heap_block *next;
    struct heap_block *prev;
} heap_block_t;

static __attribute__((aligned(HEAP_ALIGN))) uint8_t g_heap[HEAP_SIZE];
static heap_block_t *g_heap_head = NULL;
static bool g_heap_ready = false;

static uint32_t g_total_memory_bytes = 0;
static uint32_t g_total_frames = 0;
static uint32_t g_frame_bitmap[FRAME_BITMAP_BYTES / 4u];
static uint32_t g_page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
static uint32_t g_page_tables[IDENTITY_TABLES][1024] __attribute__((aligned(PAGE_SIZE)));
static bool g_paging_ready = false;

static uint32_t align_up_u32(uint32_t value, uint32_t align) {
    return (value + align - 1u) & ~(align - 1u);
}

static size_t align_up(size_t value, size_t align) {
    return (value + align - 1u) & ~(align - 1u);
}

static uint8_t *block_payload(heap_block_t *block) {
    return (uint8_t *)block + align_up(sizeof(heap_block_t), HEAP_ALIGN);
}

static heap_block_t *payload_block(void *ptr) {
    return (heap_block_t *)((uint8_t *)ptr - align_up(sizeof(heap_block_t), HEAP_ALIGN));
}

static void split_block(heap_block_t *block, size_t size) {
    size_t header_size = align_up(sizeof(heap_block_t), HEAP_ALIGN);
    size_t total_needed = header_size + align_up(size, HEAP_ALIGN);

    if (block->size <= total_needed + header_size + HEAP_ALIGN) {
        return;
    }

    uint8_t *base = (uint8_t *)block;
    heap_block_t *new_block = (heap_block_t *)(base + total_needed);
    new_block->magic = BLOCK_MAGIC;
    new_block->size = block->size - total_needed;
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;

    if (new_block->next) {
        new_block->next->prev = new_block;
    }

    block->next = new_block;
    block->size = total_needed;
}

static void merge_with_next(heap_block_t *block) {
    if (!block || !block->next || !block->next->free) {
        return;
    }

    uint8_t *expected = (uint8_t *)block + block->size;
    if ((uint8_t *)block->next != expected) {
        return;
    }

    heap_block_t *next = block->next;
    block->size += next->size;
    block->next = next->next;
    if (block->next) {
        block->next->prev = block;
    }
}

static uint32_t frame_index(uint32_t phys_addr) {
    return phys_addr / PAGE_SIZE;
}

static void set_frame(uint32_t frame) {
    g_frame_bitmap[frame / 32u] |= (1u << (frame % 32u));
}

static void clear_frame(uint32_t frame) {
    g_frame_bitmap[frame / 32u] &= ~(1u << (frame % 32u));
}

static bool test_frame(uint32_t frame) {
    return (g_frame_bitmap[frame / 32u] >> (frame % 32u)) & 1u;
}

static void reserve_range(uint32_t start_phys, uint32_t size) {
    if (size == 0) {
        return;
    }

    uint32_t start = start_phys & 0xFFFFF000u;
    uint32_t end = align_up_u32(start_phys + size, PAGE_SIZE);
    if (end > g_total_memory_bytes) {
        end = g_total_memory_bytes;
    }

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t frame = frame_index(addr);
        if (frame < g_total_frames) {
            set_frame(frame);
        }
    }
}

static void free_range(uint32_t start_phys, uint32_t size) {
    if (size == 0) {
        return;
    }

    uint32_t start = start_phys & 0xFFFFF000u;
    uint32_t end = align_up_u32(start_phys + size, PAGE_SIZE);
    if (end > g_total_memory_bytes) {
        end = g_total_memory_bytes;
    }

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t frame = frame_index(addr);
        if (frame < g_total_frames) {
            clear_frame(frame);
        }
    }
}

static void identity_map_page(uint32_t phys_addr, uint32_t flags) {
    if (phys_addr >= IDENTITY_MAP_BYTES) {
        return;
    }

    uint32_t pde = phys_addr >> 22;
    uint32_t pte = (phys_addr >> 12) & 0x3FFu;
    uint32_t table_phys = (uint32_t)&g_page_tables[pde][0];

    g_page_directory[pde] |= table_phys | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    g_page_tables[pde][pte] = (phys_addr & 0xFFFFF000u) | PAGE_PRESENT | (flags & (PAGE_WRITE | PAGE_USER));
}

static void identity_map_range(uint32_t start_phys, uint32_t size, uint32_t flags) {
    if (size == 0) {
        return;
    }

    uint32_t start = start_phys & 0xFFFFF000u;
    uint32_t end = align_up_u32(start_phys + size, PAGE_SIZE);
    if (end > IDENTITY_MAP_BYTES) {
        end = IDENTITY_MAP_BYTES;
    }

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        identity_map_page(addr, flags);
    }
}

static void enable_paging(void) {
    uint32_t pd_phys = (uint32_t)&g_page_directory[0];
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(pd_phys));

    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));
}

struct page_fault_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, orig_esp, ebx, edx, ecx, eax;
    uint32_t error_code;
    uint32_t eip, cs, eflags;
    uint32_t user_esp, user_ss;
};

extern void user_exit_return_stub(void);
extern void print_hex(uint32_t val);

static uint32_t read_cr2(void) {
    uint32_t value;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(value));
    return value;
}

void page_fault_handler(struct page_fault_frame *frame) {
    uint32_t cr2 = read_cr2();
    uint32_t err = frame->error_code;

    dbg_kv("mm", "cr2", cr2);
    dbg_kv("mm", "eip", frame->eip);
    dbg_kv("mm", "err", err);

    if (err & 4) {
        vga_set_color(COLOR_WHITE, COLOR_RED);
        vga_puts("\nProgram crashed: page fault at 0x");
        print_hex(cr2);
        vga_puts(", eip=0x");
        print_hex(frame->eip);
        vga_puts("\n");
        vga_reset_color();

        dbg_msg("mm", "user page fault, returning to kernel");
        extern uint32_t g_user_kernel_esp;
        extern void run_shell(void);
        __asm__ __volatile__(
            "mov $0x10, %%ax  \n"
            "mov %%ax, %%ds   \n"
            "mov %%ax, %%es   \n"
            "mov %%ax, %%fs   \n"
            "mov %%ax, %%gs   \n"
            "mov %%ax, %%ss   \n"
            "mov %0,   %%esp  \n"
            "sti               \n"
            "jmp *%1          \n"
            :
            : "r"(g_user_kernel_esp), "r"((uint32_t)run_shell)
            : "memory"
        );
        __builtin_unreachable();
    } else {
        vga_set_color(COLOR_WHITE, COLOR_RED);
        vga_puts("\nKernel page fault at 0x");
        print_hex(cr2);
        vga_puts(", eip=0x");
        print_hex(frame->eip);
        vga_puts("\n");
        vga_reset_color();

        dbg_msg("mm", "kernel page fault, halting");
        __asm__ __volatile__("cli; hlt");
        while (1);
    }
}

void heap_init(void) {
    if (g_heap_ready) {
        return;
    }

    g_heap_head = (heap_block_t *)g_heap;
    g_heap_head->magic = BLOCK_MAGIC;
    g_heap_head->size = HEAP_SIZE;
    g_heap_head->free = 1;
    g_heap_head->next = NULL;
    g_heap_head->prev = NULL;
    g_heap_ready = true;
    dbg_kv("mm", "heap_base", (uint32_t)g_heap);
    dbg_kv("mm", "heap_size", HEAP_SIZE);
}

static uint32_t detect_memory_bytes(uint32_t mb_magic, uint32_t mb_info_addr) {
    if (mb_magic != 0x2BADB002u || mb_info_addr == 0) {
        return 64u * 1024u * 1024u;
    }

    struct multiboot_info *mb = (struct multiboot_info *)mb_info_addr;

    if ((mb->flags & (1u << 6)) && mb->mmap_length != 0) {
        uint32_t max_end = 0;
        uint32_t offset = 0;

        while (offset < mb->mmap_length) {
            struct multiboot_mmap_entry *entry =
                (struct multiboot_mmap_entry *)(mb->mmap_addr + offset);
            uint32_t length = entry->size + sizeof(entry->size);

            if (entry->type == 1) {
                uint32_t end = entry->addr_low + entry->len_low;
                if (end > max_end) {
                    max_end = end;
                }
            }

            if (length == 0) {
                break;
            }
            offset += length;
        }

        if (max_end != 0) {
            return max_end;
        }
    }

    if (mb->flags & 0x1u) {
        uint32_t total_kb = mb->mem_lower + mb->mem_upper;
        if (total_kb != 0) {
            return total_kb * 1024u;
        }
    }

    return 64u * 1024u * 1024u;
}

void memory_init(uint32_t mb_magic, uint32_t mb_info_addr) {
    if (g_paging_ready) {
        return;
    }

    dbg_msg("mm", "initializing memory subsystem");
    dbg_kv("mm", "mb_magic", mb_magic);
    dbg_kv("mm", "mb_info", mb_info_addr);
    g_total_memory_bytes = detect_memory_bytes(mb_magic, mb_info_addr);
    if (g_total_memory_bytes > MAX_PHYS_MEM_BYTES) {
        g_total_memory_bytes = MAX_PHYS_MEM_BYTES;
    }
    g_total_memory_bytes = align_up_u32(g_total_memory_bytes, PAGE_SIZE);
    g_total_frames = g_total_memory_bytes / PAGE_SIZE;

    memset(g_frame_bitmap, 0xFF, sizeof(g_frame_bitmap));
    memset(g_page_directory, 0, sizeof(g_page_directory));
    memset(g_page_tables, 0, sizeof(g_page_tables));

    if (mb_magic == 0x2BADB002u && mb_info_addr != 0) {
        struct multiboot_info *mb = (struct multiboot_info *)mb_info_addr;

        if ((mb->flags & (1u << 6)) && mb->mmap_length != 0) {
            uint32_t offset = 0;
            while (offset < mb->mmap_length) {
                struct multiboot_mmap_entry *entry =
                    (struct multiboot_mmap_entry *)(mb->mmap_addr + offset);
                uint32_t length = entry->size + sizeof(entry->size);

                if (entry->type == 1) {
                    uint32_t start = entry->addr_low;
                    uint32_t size = entry->len_low;
                    if (start < g_total_memory_bytes) {
                        if (start + size > g_total_memory_bytes) {
                            size = g_total_memory_bytes - start;
                        }
                        free_range(start, size);
                    }
                }

                if (length == 0) {
                    break;
                }
                offset += length;
            }
        } else {
            free_range(0x00100000u, g_total_memory_bytes > 0x00100000u ? (g_total_memory_bytes - 0x00100000u) : 0);
        }
    } else {
        free_range(0x00100000u, g_total_memory_bytes > 0x00100000u ? (g_total_memory_bytes - 0x00100000u) : 0);
    }

    reserve_range(0x00000000u, 0x00100000u);
    if ((uint32_t)&kernel_end > 0x00100000u) {
        reserve_range(0x00100000u, (uint32_t)&kernel_end - 0x00100000u);
    }
    reserve_range(USER_PROGRAM_BASE, USER_PROGRAM_MAX_SIZE);
    reserve_range(USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE);

    uint32_t identity_bytes = g_total_memory_bytes;
    if (identity_bytes > IDENTITY_MAP_BYTES) {
        identity_bytes = IDENTITY_MAP_BYTES;
    }
    identity_map_range(0x00000000u, identity_bytes, PAGE_WRITE);
    paging_mark_user_range(USER_PROGRAM_BASE, USER_PROGRAM_MAX_SIZE);
    paging_mark_user_range(USER_STACK_TOP - USER_STACK_SIZE, USER_STACK_SIZE);

    enable_paging();
    g_paging_ready = true;

    heap_init();

    dbg_kv("mm", "total_bytes", g_total_memory_bytes);
    dbg_kv("mm", "frames", g_total_frames);
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t frame = 0; frame < g_total_frames; frame++) {
        if (!test_frame(frame)) {
            set_frame(frame);
            dbg_kv("mm", "alloc_frame", frame * PAGE_SIZE);
            return frame * PAGE_SIZE;
        }
    }
    dbg_msg("mm", "frame allocation failed");
    return 0;
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = frame_index(phys_addr);
    if (frame < g_total_frames) {
        clear_frame(frame);
        dbg_kv("mm", "free_frame", phys_addr);
    }
}

void paging_mark_user_range(uint32_t phys_addr, size_t size) {
    if (size == 0) {
        return;
    }

    uint32_t start = phys_addr & 0xFFFFF000u;
    uint32_t end = align_up_u32((uint32_t)(phys_addr + (uint32_t)size), PAGE_SIZE);

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        identity_map_page(addr, PAGE_WRITE | PAGE_USER);
    }
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    if (!g_heap_ready) {
        heap_init();
    }

    size_t aligned_size = align_up(size, HEAP_ALIGN);
    size_t header_size = align_up(sizeof(heap_block_t), HEAP_ALIGN);
    heap_block_t *cur = g_heap_head;

    while (cur) {
        if (cur->magic == BLOCK_MAGIC && cur->free && cur->size >= aligned_size + header_size) {
            split_block(cur, aligned_size);
            cur->free = 0;
            return block_payload(cur);
        }
        cur = cur->next;
    }

    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) {
        return;
    }

    heap_block_t *block = payload_block(ptr);
    if (block->magic != BLOCK_MAGIC) {
        return;
    }

    block->free = 1;
    merge_with_next(block);
    if (block->prev && block->prev->free) {
        merge_with_next(block->prev);
    }
}

void *kcalloc(size_t count, size_t size) {
    if (count != 0 && size > ((size_t)-1) / count) {
        return NULL;
    }

    size_t total = count * size;
    uint8_t *ptr = (uint8_t *)kmalloc(total);
    if (!ptr) {
        return NULL;
    }
    memset(ptr, 0, total);
    return ptr;
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) {
        return kmalloc(size);
    }

    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    heap_block_t *block = payload_block(ptr);
    if (block->magic != BLOCK_MAGIC) {
        return NULL;
    }

    size_t old_size = block->size - align_up(sizeof(heap_block_t), HEAP_ALIGN);
    if (size <= old_size) {
        return ptr;
    }

    void *new_ptr = kmalloc(size);
    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);
    return new_ptr;
}

void *memset(void *dst, int c, size_t n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) {
        *p++ = (uint8_t)c;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) {
        return dst;
    }

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }

    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;

    while (n--) {
        if (*pa != *pb) {
            return (int)*pa - (int)*pb;
        }
        pa++;
        pb++;
    }

    return 0;
}

void *pmm_alloc_page(void) {
    return (void*)pmm_alloc_frame();
}

uint32_t page_get_phys(uint32_t *pd, uint32_t vaddr) {
    uint32_t pde = vaddr >> 22;
    uint32_t pte = (vaddr >> 12) & 0x3FF;
    if (!(pd[pde] & PAGE_PRESENT)) return 0;
    uint32_t *pt = (uint32_t*)(pd[pde] & 0xFFFFF000);
    if (!(pt[pte] & PAGE_PRESENT)) return 0;
    return (pt[pte] & 0xFFFFF000) | (vaddr & 0xFFF);
}

void page_map(uint32_t *pd, uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pde = vaddr >> 22;
    uint32_t pte = (vaddr >> 12) & 0x3FF;
    
    if (!(pd[pde] & PAGE_PRESENT)) {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) return;
        uint32_t *pt = (uint32_t*)pt_phys;
        memset(pt, 0, 4096);
        pd[pde] = pt_phys | flags | PAGE_PRESENT;
    }
    uint32_t *pt = (uint32_t*)(pd[pde] & 0xFFFFF000);
    pt[pte] = (paddr & 0xFFFFF000) | flags | PAGE_PRESENT;
}

uint32_t *kernel_pd = g_page_directory;
