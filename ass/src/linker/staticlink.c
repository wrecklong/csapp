#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <headers/linker.h>
#include <headers/common.h>
#include <headers/cpu.h>

#define MAX_SYMBOL_MAP_LENGTH 64
#define MAX_SECTION_BUFFER_LENGTH 64


typedef struct SMAP_T
{
   elf_t* elf;
   st_entry_t *src;
   st_entry_t *dst;
}smap_t;

static void symbol_processing(elf_t **srcs, int num_srcs, elf_t *dst,smap_t *smap_table, int *smap_count);
static void simple_resolution(st_entry_t *sym, elf_t *sym_elf, smap_t *candidate);

static void compute_section_header(elf_t *dst, smap_t *smap_table, int *smap_count);
static void merge_section(elf_t **srcs, int num_srcs, elf_t *dst, smap_t *smap_table, int *smap_count);

void link_elf(elf_t **srcs, int num_srcs,elf_t *dst);

static void R_X86_64_32_handler(elf_t *dst,sh_entry_t * sh , int row_referencing, int col_referencing, int addend, st_entry_t *sym_referenced);
static void R_X86_64_PC32_handler(elf_t *dst,sh_entry_t * sh , int row_referencing, int col_referencing, int addend, st_entry_t *sym_referenced);

typedef void (*rela_handler_t)(elf_t *dst, sh_entry_t* sh, int row_referencing, int col_referencing, int addend, st_entry_t *sym_referenced);

static rela_handler_t handler_table[3] ={
    &R_X86_64_32_handler,
    &R_X86_64_PC32_handler,
    &R_X86_64_PC32_handler,
};


static const char *get_stb_string(st_bind_t bind);
static const char *get_stt_string(st_type_t type);

static int symbol_precedence(st_entry_t *sym)
{
    // 
    /*
        bind    type   shndex           prec
        --------------------------------------------
        global  notype  undef           0-undefined
        global  object  common          1-tentative
        global  object  data,bss,todata 2-defined
        global  func    text            2-defined

    */

   if (strcmp(sym->st_shndx, "SHN_UNDEF") == 0 && sym->type == STT_NOTYPE)
   {
        return 0;
   }

   if (strcmp(sym->st_shndx, "COMMON") == 0 && sym->type == STT_OBJECT)
   {
      return 1;
   }

   if ((strcmp(sym->st_shndx, ".text") == 0 && sym->type == STT_FUNC)||
       (strcmp(sym->st_shndx, ".data") == 0 && sym->type == STT_OBJECT)||
       (strcmp(sym->st_shndx, ".rodata") == 0 && sym->type == STT_OBJECT)||
       (strcmp(sym->st_shndx, ".bss") == 0 && sym->type == STT_OBJECT))
   {
       return 2;
   }

    debug_printf(DEBUG_LINKER, "symbol resolution:cannot determine the symbol \"%s\" precedence", sym->st_name);
    exit(0);
}

static void simple_resolution(st_entry_t *sym, elf_t *sym_elf, smap_t *candidate)
{
    int pre1 = symbol_precedence(sym);
    int pre2 = symbol_precedence(candidate->src);

    if (pre1 == 2 && pre2 == 2)
    {
        debug_printf(DEBUG_LINKER, "symbol resolution: strong symbol \"%s\" is redeclared\n", sym->st_name);
        exit(0);
    }

    if (pre1 != 2 && pre2 != 2)
    {
        if (pre1 > pre2)
        {
            candidate->src = sym;
            candidate->elf = sym_elf;
        }
        return;
    }

    if (pre1 == 2)
    {
        //select sym as best match
        candidate->src = sym;
        candidate->elf = sym_elf;
    }
}


static void  symbol_processing(elf_t **srcs, int num_srcs, elf_t *dst, smap_t *smap_table, int* smap_count)
{
    for (int i = 0; i < num_srcs; i++)
    {
        elf_t *elfp = srcs[i];

        for (int j = 0; j < elfp->symtab_count; j++)
        {
            st_entry_t *sym = &(elfp->symtab[j]);

            if (sym->bind == STB_LOCAL)
            {
                assert(*smap_count < MAX_SYMBOL_MAP_LENGTH);
                //even if local symbol has the same name, just insert it into dst
                smap_table[*smap_count].src = sym;
                smap_table[*smap_count].elf = elfp;
                
                (*smap_count)++;
            }
            else if (sym->bind == STB_GLOBAL)
            {
                for (int k = 0; k < *smap_count; k++)
                {
                    st_entry_t *candidate = smap_table[k].src;
                    if (candidate->bind == STB_GLOBAL && (strcmp(candidate->st_name, sym->st_name)) == 0)
                    {  
                        simple_resolution(sym, elfp, &smap_table[k]);
                        goto NEXT_SYMBOL_PROCESS;
                    }

                }

                assert(*smap_count < MAX_SYMBOL_MAP_LENGTH);
                smap_table[*smap_count].src = sym;
                smap_table[*smap_count].elf = elfp;
                (*smap_count)++;
            }
             NEXT_SYMBOL_PROCESS:
             ;
        }
    }

    for(int i = 0 ; i < *smap_count; i ++)
    {
        st_entry_t * s = smap_table[i].src;

        assert(strcmp(s->st_name, "SHN_UNDEF") != 0);
        assert(s->type != STT_NOTYPE);

        if (strcmp(s->st_shndx, "COMMON") == 0)
        {
            char *bss = ".bss";
            for (int j = 0; j < MAX_CHAR_SECTION_NAME; j++)
            {
                if (j < 4)
                {
                    s->st_shndx[j] = bss[j];
                }
                else
                {
                    s->st_shndx[j] = '\0';
                }
            }
            s->st_value = 0;
        }
    }
}

static uint64_t get_symbol_runtime_address(elf_t *dst, st_entry_t *sym)
{
    uint64_t base = 0x00400000;
    uint64_t text_base = base;
    uint64_t rodata_base = base;
    uint64_t data_base = base;

    int inst_size = sizeof(inst_t);
    int data_size = sizeof(uint64_t);

    for (int i = 0; i < dst->sht_count; i++)
    {
        if (strcmp(dst->sht[i].sh_name, ".text") == 0)
        {
            rodata_base = text_base + dst->sht[i].sh_size * inst_size;
            data_base = rodata_base;
        }
        else if (strcmp(dst->sht[i].sh_name, ".rodata") == 0)
        {
            data_base = rodata_base + dst->sht[i].sh_size * data_size;
        }
    }

    if (strcmp(sym->st_shndx, ".text") == 0)
    {
        return text_base + inst_size * sym->st_value;
    }
    else if (strcmp(sym->st_shndx, ".rodata") == 0)
    {
         return rodata_base + data_size * sym->st_value;
    }
    else if (strcmp(sym->st_shndx, ".data") == 0)
    {
         return data_base + data_size * sym->st_value;
    }
    
    return 0xffffffffffffffff;
}

static void write_relocation(char *dst, uint64_t val)
{
    char temp[20];
    sprintf(temp, "0x%016lx", val);

    for(int i = 0; i < 18; i++)
    {
        dst[i] = temp[i];
    }
}

static void R_X86_64_32_handler(elf_t *dst,sh_entry_t * sh ,int row_referencing, int col_referencing, int addend, st_entry_t *sym_referenced)
{
    uint64_t sym_address = get_symbol_runtime_address(dst, sym_referenced);

    char *s = &dst->buffer[sh->sh_offset + row_referencing][col_referencing];
    write_relocation(s, sym_address);
}

static void R_X86_64_PC32_handler(elf_t *dst,sh_entry_t * sh , int row_referencing, int col_referencing, int addend, st_entry_t *sym_referenced)
{

    assert(strcmp(sh->sh_name, ".text") == 0);
    uint64_t sym_address = get_symbol_runtime_address(dst, sym_referenced);

    uint64_t rip_value = 0x00400000 + (row_referencing + 1) * sizeof(inst_t);

    char *s = &dst->buffer[sh->sh_offset + row_referencing][col_referencing];
    write_relocation(s, sym_address - rip_value);
}

static void relocation_processing(elf_t **srcs, int num_srcs, elf_t *dst, smap_t *smap_table, int* smap_count)
{

    sh_entry_t *eof_text_sh = NULL;
    sh_entry_t *eof_data_sh = NULL;
    for (int i = 0; i < dst->sht_count; i++)
    {
        if (strcmp(dst->sht[i].sh_name, ".text") == 0)
        {
            eof_text_sh = &(dst->sht[i]);
        }
        else if (strcmp(dst->sht[i].sh_name, ".data") == 0)
        {
            eof_data_sh = &(dst->sht[i]);
        }
    }

    for (int i = 0; i < num_srcs; i++)
    {
        elf_t *elf = srcs[i];

        for (int j = 0; j < elf->reltext_count; j++)
        {
            rl_entry_t *r = &elf->reltext[j];

            for (int k = 0; k < elf->symtab_count; k++)
            {
                st_entry_t* sym = &elf->symtab[k];

                if (strcmp(sym->st_shndx, ".text") == 0)
                {
                    int sym_text_start = sym->st_value;
                    int sym_text_end =  sym->st_value + sym->st_size;

                    if (sym_text_start <= r->r_row && r->r_row <= sym_text_end)
                    {
                        int smap_found = 0;

                        for (int t = 0; t < *smap_count; t++)
                        {
                            //find the referencing symbol
                            if (smap_table[t].src == sym)
                            {
                               smap_found = 1;
                               st_entry_t *eof_referencing = smap_table[t].dst;
                              // search the being referenced symbol
                               for (int u = 0; u <*smap_count; u++)
                               {
                                    if (strcmp(elf->symtab[r->sym].st_name, smap_table[u].dst->st_name) == 0 &&
                                        smap_table[u].dst->bind == STB_GLOBAL)
                                    {
                                        st_entry_t* eof_referenced = smap_table[u].dst;
                                        (handler_table[(int)r->type])(
                                            dst,
                                            eof_text_sh,
                                            r->r_row - sym->st_value + eof_referencing->st_value,
                                            r->r_col,
                                            r->r_addend,
                                            eof_referenced);
                                    }
                               } 
                            } 
                        }

                        assert(smap_found == 1);
                    }
                }
            }
        }

        for (int j = 0; j < elf->reldata_count; j++)
        {
            rl_entry_t *r = &elf->reldata[j];

            for (int k = 0; k < elf->symtab_count; k++)
            {
                st_entry_t* sym = &elf->symtab[k];

                if (strcmp(sym->st_shndx, ".data") == 0)
                {
                    int sym_data_start = sym->st_value;
                    int sym_data_end =  sym->st_value + sym->st_size;

                    if (sym_data_start <= r->r_row && r->r_row <= sym_data_end)
                    {
                        int smap_found = 0;

                        for (int t = 0; t < *smap_count; t++)
                        {
                            //find the referencing symbol
                            if (smap_table[t].src == sym)
                            {
                               smap_found = 1;
                               st_entry_t *eof_referencing = smap_table[t].dst;
                              // search the being referenced symbol
                               for (int u = 0; u <*smap_count; u++)
                               {
                                    if (strcmp(elf->symtab[r->sym].st_name, smap_table[u].dst->st_name) == 0 &&
                                        smap_table[u].dst->bind == STB_GLOBAL)
                                    {
                                        st_entry_t* eof_referenced = smap_table[u].dst;
                                        (handler_table[(int)r->type])(
                                            dst,
                                            eof_data_sh,
                                            r->r_row - sym->st_value + eof_referencing->st_value,
                                            r->r_col,
                                            r->r_addend,
                                            eof_referenced);
                                    }
                               } 
                            } 
                        }

                        assert(smap_found == 1);
                    }
                }
            }
        }
    }
}


static void compute_section_header(elf_t *dst, smap_t *smap_table, int *smap_count)
{
    int count_text = 0, count_rodata = 0, count_data = 0;
    for (int i = 0; i < *smap_count; i++)
    {
        st_entry_t *sym = smap_table[i].src;

        if (strcmp(sym->st_shndx, ".text") == 0)
        {
            count_text += sym->st_size;
        }
        else if (strcmp(sym->st_shndx, ".rodata") == 0)
        {
            count_rodata += sym->st_size;
        }
        else if (strcmp(sym->st_shndx, ".data") == 0)
        {
            count_data += sym->st_size;
        }
    }

    // count the section with symbol_table
    dst->sht_count = (count_text != 0) + (count_rodata != 0) + (count_data != 0) + 1;
    // count the total lines
    dst->line_count = 1 + 1 + dst->sht_count + count_text + count_rodata + count_data + *smap_count;

    sprintf(dst->buffer[0], "%ld", dst->line_count);
    sprintf(dst->buffer[1], "%ld", dst->sht_count);

    uint64_t text_runtime_addr = 0x00400000;
    uint64_t rodata_runtime_addr = text_runtime_addr + count_text * MAX_INSTRUCTION_CHAR * sizeof(char);
    uint64_t data_runtime_addr =  rodata_runtime_addr + count_rodata * sizeof(uint64_t);
    uint64_t symtab_runtime_addr = 0; //For EOF, symtab is not loaded into memory but still on disk

    assert(dst->sht == NULL);
    dst->sht = malloc(dst->sht_count * sizeof(sh_entry_t));

    uint64_t section_offset = 1 + 1 + dst->sht_count;
    int sh_index = 0;

    sh_entry_t *sh = NULL;

    if (count_text > 0)
    {
        assert(sh_index < dst->sht_count);

        sh = &(dst->sht[sh_index]);

        strcpy(sh->sh_name, ".text");
        sh->sh_addr = text_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_text;

        sprintf(dst->buffer[2 + sh_index], "%s,0x%lx,%ld,%ld", sh->sh_name, sh->sh_addr,
        sh->sh_offset,sh->sh_size);

        sh_index ++;
        section_offset += sh->sh_size;
    }

    if (count_rodata > 0)
    {
        assert(sh_index < dst->sht_count);

        sh = &(dst->sht[sh_index]);

        strcpy(sh->sh_name, ".rodata");
        sh->sh_addr = rodata_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_rodata;

        sprintf(dst->buffer[2 + sh_index], "%s,0x%lx,%ld,%ld", sh->sh_name, sh->sh_addr,
        sh->sh_offset,sh->sh_size);

        sh_index ++;
        section_offset += sh->sh_size;
    }

    
    if (count_data > 0)
    {
        assert(sh_index < dst->sht_count);

        sh = &(dst->sht[sh_index]);

        strcpy(sh->sh_name, ".data");
        sh->sh_addr = data_runtime_addr;
        sh->sh_offset = section_offset;
        sh->sh_size = count_data;

        sprintf(dst->buffer[2 + sh_index], "%s,0x%lx,%ld,%ld", sh->sh_name, sh->sh_addr,
        sh->sh_offset,sh->sh_size);

        sh_index ++;
        section_offset += sh->sh_size;
    }

    assert(sh_index < dst->sht_count);

    sh = &(dst->sht[sh_index]);

    strcpy(sh->sh_name, ".symtab");
    sh->sh_addr = symtab_runtime_addr;
    sh->sh_offset = section_offset;
    sh->sh_size = *smap_count;

    sprintf(dst->buffer[2 + sh_index], "%s,0x%lx,%ld,%ld", sh->sh_name, sh->sh_addr,
    sh->sh_offset,sh->sh_size);

    assert(sh_index + 1 == dst->sht_count);

    if ((DEBUG_VERBOSE_SET & DEBUG_LINKER) != 0)
    {
        printf("----------------------\n");
        printf("Destination ELF's SHT in BUFFER:\n");

        for (int i = 0; i < 2 + dst->sht_count; i++)
        {
            printf("%s\n", dst->buffer[i]);
        }
    }
}

static void merge_section(elf_t **srcs, int num_srcs, elf_t *dst, smap_t* smap_table, int *smap_count)
{
    int line_written = 1 + 1 + dst->sht_count;
    int symt_writen = 0;
    int sym_section_offset = 0;

    // iterate every section
    for (int section_index = 0; section_index < dst->sht_count; section_index++)
    {
        sh_entry_t * target_sh = &dst->sht[section_index];
        sym_section_offset = 0;
        
        debug_printf(DEBUG_LINKER, "merging section '%s'\n", target_sh->sh_name);

        target_sh->sh_addr = 0;
        
        // scan every ELF file
        for (int i = 0; i < num_srcs; i++)
        {
            debug_printf(DEBUG_LINKER, "\tfrom source elf [%d]\n", i);
            int src_section_index = -1;
            
            // scan every section in this file
            for (int j = 0; j < srcs[i]->sht_count; j++)
            {
                if (strcmp(target_sh->sh_name, srcs[i]->sht[j].sh_name) == 0)
                {
                    src_section_index = j;
                    break;
                }
            }

            if (src_section_index == -1)
            {
                //search for next ELF
                continue;
            }
            else
            {
                for(int j = 0; j < srcs[i]->symtab_count; j++)
                {
                    st_entry_t *sym = &srcs[i]->symtab[j];

                    if (strcmp(sym->st_shndx, target_sh->sh_name) == 0)
                    {
                        for (int k = 0; k < *smap_count; k++)
                        {
                            if (sym == smap_table[k].src)
                            {                         
                                //copy this symbol from srcs[i].buffer into dst.buffer
                                // srcs[i].buffer[sh_offset + st_value, sh_offset+ st_value + st_size]
                                debug_printf(DEBUG_LINKER, "\t\tsymbol '%s'\n", sym->st_name);
                               
                                for (int t = 0; t < sym->st_size; t++)
                                {
                                    int dst_index = line_written + t;
                                    int src_index = srcs[i]->sht[src_section_index].sh_offset + sym->st_value + t;

                                    assert(dst_index < MAX_ELF_FILE_LENGTH);
                                    assert(src_index < MAX_ELF_FILE_LENGTH);
                                
                                    strcpy(
                                        dst->buffer[dst_index],
                                        smap_table[k].elf->buffer[src_index]
                                    );
                                }

                                strcpy(dst->symtab[symt_writen].st_name, sym->st_name);
                                strcpy(dst->symtab[symt_writen].st_shndx, sym->st_shndx);
                                dst->symtab[symt_writen].bind = sym->bind;
                                dst->symtab[symt_writen].type = sym->type;
                                dst->symtab[symt_writen].st_value = sym_section_offset;
                                dst->symtab[symt_writen].st_size = sym->st_size;

                                smap_table[k].dst = &dst->symtab[symt_writen];

                                symt_writen++;
                                line_written += sym->st_size;
                                sym_section_offset += sym->st_size;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < dst->symtab_count; i++)
    {
        st_entry_t * sym = &dst->symtab[i];
        sprintf(dst->buffer[line_written], "%s,%s,%s,%s,%ld,%ld",
        sym->st_name, get_stb_string(sym->bind), get_stt_string(sym->type),
            sym->st_shndx, sym->st_value, sym->st_size);
        
        line_written++;
    }
    assert(line_written == dst->line_count);
}


static const char *get_stb_string(st_bind_t bind)
{
    switch (bind)
    {
        case STB_GLOBAL:
            return "STB_GLOBAL";
        case STB_LOCAL:
            return "STB_LOCAL";
        case STB_WEAK:
            return "STB_WEAK";
        default:
            printf("incorrect symbol bind\n");
            exit(0);
    }
}

static const char *get_stt_string(st_type_t type)
{
    switch (type)
    {
        case STT_NOTYPE:
            return "STT_NOTYPE";
        case STT_OBJECT:
            return "STT_OBJECT";
        case STT_FUNC:
            return "STT_FUNC";
        default:
            printf("incorrect symbol type\n");
            exit(0);
    }
}


void link_elf(elf_t **srcs, int num_srcs, elf_t *dst)
{
    memset(dst, 0 ,sizeof(elf_t));

    int smap_count = 0;
    smap_t smap_table[MAX_SYMBOL_MAP_LENGTH];

    symbol_processing(srcs, num_srcs, dst, smap_table, &smap_count);

    printf("--------------------------------\n");

    for (int i = 0; i < smap_count; i++)
    {
        st_entry_t *ste = smap_table[i].src;

        debug_printf(DEBUG_LINKER, "%s\t%d\t%d\t%s\t%d\t%d\n", 
        ste->st_name, 
        ste->bind, 
        ste->type,
        ste->st_shndx,
        ste->st_value,
        ste->st_size);
    }
    

    compute_section_header(dst, smap_table, &smap_count);

    dst->symtab_count = smap_count;

    dst->symtab = malloc(dst->symtab_count * sizeof(st_entry_t));

    merge_section(srcs, num_srcs, dst, smap_table, &smap_count);

    printf("------------------------------\n");
    printf("after merging the sections\n");

    for (int i = 0; i < dst->line_count; i++)
    {
        printf("%s\n", dst->buffer[i]);
    }

    relocation_processing(srcs, num_srcs, dst, smap_table, &smap_count);

    if((DEBUG_VERBOSE_SET & DEBUG_LINKER) != 0)
    {
        printf("---\nfinal output EOF:\n");
        for (int i = 0; i < dst->line_count; i++)
        {
            printf("%s\n", dst->buffer[i]);
        }
    }
}