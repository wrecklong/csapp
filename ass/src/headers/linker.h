#ifndef LINK_H
#define LINK_H

#include <stdint.h>
#include <stdlib.h>

#define MAX_CHAR_SECTION_NAME (32)

typedef struct SH_ENTRY
{
    char sh_name[MAX_CHAR_SECTION_NAME];
    uint64_t sh_addr;
    uint64_t sh_offset; //line offset or effective line index
    uint64_t sh_size;
}sh_entry_t;

#define MAX_CHAR_SYMBOL_NAME (64)

typedef enum
{
   STB_LOCAL,
   STB_GLOBAL,
   STB_WEAK
}st_bind_t;

typedef enum
{
    STT_NOTYPE,
    STT_OBJECT,
    STT_FUNC
}st_type_t;

typedef struct ST_ENTRY
{
    char st_name[MAX_CHAR_SYMBOL_NAME];
    st_bind_t bind;
    st_type_t type;
    char st_shndx[MAX_CHAR_SECTION_NAME];
    uint64_t st_value; // in_section offset
    uint64_t st_size; // count of lines of symbol

}st_entry_t;

#define MAX_ELF_FILE_LENGTH (64) //LINES
#define MAX_ELF_FILE_WIDTH (128)

typedef struct 
{
    char buffer[MAX_ELF_FILE_LENGTH][MAX_ELF_FILE_WIDTH];
    uint64_t line_count;

    sh_entry_t *sht;

    uint64_t symtab_count;
    st_entry_t *symtab;

}elf_t;

void parse_elf(char *filename, elf_t *elf);

void free_elf(elf_t *elf);

void link_elf(elf_t **srcs, int num_srcs,elf_t *dst);

#endif
