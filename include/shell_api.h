#ifndef SHELL_API_H
#define SHELL_API_H

#include <stdint.h>
#include <stdbool.h>

void ext2_cd(const char *path);
void ext2_ls(uint32_t dir_inode_num);
void ext2_cat(const char *filename);
bool load_file_to_memory(uint32_t inode_num, uint8_t *dest_addr);
bool load_elf_program_from_inode(uint32_t inode_num, uint32_t *entry_point_out);
uint32_t resolve_path(const char *path);

#endif
