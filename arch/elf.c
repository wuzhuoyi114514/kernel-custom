#include <stdbool.h>
#include <stdint.h>

#include "debug.h"
#include "elf.h"
#include "ext2.h"
#include "fs_runtime.h"
#include "memory.h"
#include "shell_api.h"

static bool validate_segment(uint32_t vaddr, uint32_t memsz) {
    if (memsz == 0) {
        return true;
    }

    uint32_t region_start = USER_PROGRAM_BASE;
    uint32_t region_end = USER_PROGRAM_BASE + USER_PROGRAM_MAX_SIZE;
    uint32_t seg_end = vaddr + memsz;

    if (vaddr < region_start) {
        return false;
    }
    if (seg_end < vaddr) {
        return false;
    }
    if (seg_end > region_end) {
        return false;
    }

    return true;
}

#include "vfs.h"

bool load_elf_program_from_vfs(vfs_node_t *node, uint32_t *entry_point_out) {
    if (entry_point_out == 0 || node == 0) {
        return false;
    }

    dbg_kv("elf", "vfs_id", node->internal_id);

    if (node->type != VFS_TYPE_FILE) {
        dbg_msg("elf", "target is not a regular file");
        return false;
    }

    if (node->size < sizeof(struct elf32_ehdr)) {
        dbg_msg("elf", "file too small");
        return false;
    }

    uint8_t *image = (uint8_t *)kmalloc(node->size);
    if (image == 0) {
        dbg_msg("elf", "kmalloc failed");
        return false;
    }

    if (!vfs_read(node, 0, node->size, image)) {
        dbg_msg("elf", "file load failed");
        kfree(image);
        return false;
    }

    struct elf32_ehdr *ehdr = (struct elf32_ehdr *)image;
    if (*(uint32_t *)ehdr->e_ident != ELF32_MAGIC) {
        dbg_msg("elf", "bad magic");
        kfree(image);
        return false;
    }
    if (ehdr->e_ident[4] != ELF32_CLASS_32 ||
        ehdr->e_ident[5] != ELF32_DATA_LSB ||
        ehdr->e_ident[6] != ELF32_VERSION_CURRENT) {
        dbg_msg("elf", "bad ident");
        kfree(image);
        return false;
    }
    if (ehdr->e_machine != ELF32_MACHINE_386 || ehdr->e_version != ELF32_VERSION_CURRENT) {
        dbg_msg("elf", "unsupported target");
        kfree(image);
        return false;
    }
    if (ehdr->e_ehsize != sizeof(struct elf32_ehdr)) {
        dbg_msg("elf", "unexpected ehdr size");
        kfree(image);
        return false;
    }
    if (ehdr->e_phentsize != sizeof(struct elf32_phdr)) {
        dbg_msg("elf", "unexpected phdr size");
        kfree(image);
        return false;
    }

    uint32_t ph_end = ehdr->e_phoff + (uint32_t)ehdr->e_phnum * sizeof(struct elf32_phdr);
    if (ph_end < ehdr->e_phoff || ph_end > node->size) {
        dbg_msg("elf", "invalid phdr table");
        kfree(image);
        return false;
    }

    for (uint32_t i = 0; i < ehdr->e_phnum; i++) {
        struct elf32_phdr *phdr = (struct elf32_phdr *)(image + ehdr->e_phoff + i * sizeof(struct elf32_phdr));

        if (phdr->p_type != ELF32_PT_LOAD) {
            continue;
        }

        if (phdr->p_offset + phdr->p_filesz < phdr->p_offset) {
            dbg_msg("elf", "segment overflow");
            kfree(image);
            return false;
        }
        if (phdr->p_offset + phdr->p_filesz > node->size) {
            dbg_msg("elf", "segment outside file");
            kfree(image);
            return false;
        }
        if (phdr->p_memsz < phdr->p_filesz) {
            dbg_msg("elf", "memsz < filesz");
            kfree(image);
            return false;
        }
        if (!validate_segment(phdr->p_vaddr, phdr->p_memsz)) {
            dbg_msg("elf", "segment outside user range");
            kfree(image);
            return false;
        }

        if (phdr->p_memsz != 0) {
            paging_mark_user_range(phdr->p_vaddr, (phdr->p_memsz + 0xFFFu) & ~0xFFFu);
        }

        memcpy((void *)phdr->p_vaddr, image + phdr->p_offset, phdr->p_filesz);
        if (phdr->p_memsz > phdr->p_filesz) {
            memset((void *)(phdr->p_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
        }

        dbg_kv("elf", "load_vaddr", phdr->p_vaddr);
        dbg_kv("elf", "load_filesz", phdr->p_filesz);
        dbg_kv("elf", "load_memsz", phdr->p_memsz);
    }

    *entry_point_out = ehdr->e_entry;
    dbg_kv("elf", "entry", *entry_point_out);

    kfree(image);
    return true;
}
