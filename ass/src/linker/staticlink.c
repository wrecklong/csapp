#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <headers/linker.h>
#include <headers/common.h>

#define MAX_SYMBOL_MAP_LENGTH 64
#define MAX_SECTION_BUFFER_LENGTH 64


typedef struct SMAP_T
{
   elf_t* elf;
   st_entry_t *src;
   st_entry_t *dst;
}smap_t;

static void  symbol_processing(elf_t **srcs, int num_srcs, elf_t *dst, smap_t *smap_table, int* smap_count);

void link_elf(elf_t **srcs, int num_srcs,elf_t *dst);

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

}