#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <headers/linker.h>
#include <headers/common.h>

static int parse_table_entry(char *str, char ***ent)
{
    int count_col = 1;
    int len = strlen(str);

    for(int i = 0; i < len; i++)
    {
        if (str[i] == ',')
        {
            count_col++;
        }
    }

    char **arr = malloc(count_col * sizeof(char *));
    *ent = arr;

    int col_index = 0;
    int col_width = 0;
    char col_buf[32];

    for(int i = 0; i < len + 1; i++)
    {
        if (str[i] == ',' || str[i] == '\0')
        {
            assert(col_index < count_col);

            char *col = malloc((col_width + 1) * sizeof(char));

            for(int j = 0; j < col_width; j++)
            {
                col[j] = col_buf[j];
            }

            col[col_width] = '\0';

            arr[col_index] = col;
            col_index ++;
            col_width = 0;
        }

        else 
        {
            assert(col_width < 32);
            col_buf[col_width] = str[i];
            col_width ++;
        }
    }

    return count_col;
}

static void free_table_entry(char **ent, int n)
{
    for(int i = 0; i < n; i++)
    {
        free(ent[i]);
    }

    free(ent);
}

static void parse_sh(char *str, sh_entry_t* sh)
{
    char **cols;
    int num_cols = parse_table_entry(str, &cols);

    assert(num_cols == 4);

    assert(sh != NULL);
    strcpy(sh->sh_name,cols[0]);
    sh->sh_addr = string2uint(cols[1]);
    sh->sh_offset = string2uint(cols[2]);
    sh->sh_size = string2uint(cols[3]);

    free_table_entry(cols, num_cols);
}

static void print_sh_entry(sh_entry_t *sh)
{
    debug_printf(DEBUG_LINKER, "%s\t%x\t%d\t%d\n", 
    sh->sh_name,sh->sh_addr, sh->sh_offset, sh->sh_size);
}

static void parse_symtab(char *str, st_entry_t *ste)
{
    char **cols;
    int num_cols = parse_table_entry(str, &cols); 
    assert(num_cols == 6);

    assert(ste != NULL);

    strcpy(ste->st_name, cols[0]);
    
    if (strcmp(cols[1], "STB_LOCAL") == 0)
    {
        ste->bind = STB_LOCAL;
    }
    else if (strcmp(cols[1], "STB_GLOBAL") == 0)
    {
        ste->bind = STB_GLOBAL;
    }
    else if (strcmp(cols[1], "STB_WEAK") == 0)
    {
        ste->bind = STB_WEAK;
    }
    else
    {
        printf("symbol bind is neither local, global, weak\n");
        exit(0);
    }

    if (strcmp(cols[2], "STT_NOTYPE") == 0)
    {
        ste->type = STT_NOTYPE;
    }
    else if (strcmp(cols[2], "STT_OBJECT") == 0)
    {
        ste->type = STT_OBJECT;
    }
    else if (strcmp(cols[2], "STT_FUNC") == 0)
    {
         ste->type = STT_FUNC;
    }
    else
    {
        printf("symbol TYPE is neither notype, object, func\n");
        exit(0);
    }

    strcpy(ste->st_shndx, cols[3]);
    ste->st_value = string2uint(cols[4]);
    ste->st_size = string2uint(cols[5]);

    free_table_entry(cols, num_cols);

}

static void print_symtab_entry(st_entry_t* ste)
{
    debug_printf(DEBUG_LINKER, "%s\t%d\t%d\t%s\t%d\t%d\n", 
    ste->st_name, 
    ste->bind, 
    ste->type,
    ste->st_shndx,
    ste->st_value,
    ste->st_size);
}

static int read_elf(const char *filename, uint64_t bufaddr)
{
    FILE *fp;
    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        debug_printf(DEBUG_LINKER, "Unable to open file %s\n", filename);
        exit(1);
    }

    char line[MAX_ELF_FILE_WIDTH];
    int line_counter = 0;

    while(fgets(line, MAX_ELF_FILE_WIDTH, fp) != NULL)
    {
        int len = strlen(line);
        if ((len == 0)|| 
            (len >= 1 && (line[0] == '\n' || line[0] == '\r')) ||
            (len >= 2 && (line[0] == '/' && line[1] == '/')))
            {
                continue;
            }

        // check if it is empty or white line
        int iswhite = 1;
        for (int i = 0; i < len; i++)
        {
            iswhite = iswhite && (line[i] == ' ' ||line[i] == '\t' || line[i] == '\r' );
        }
        if (iswhite == 1)
        {
            continue;
        }
        
        if (line_counter < MAX_ELF_FILE_LENGTH)
        {
            uint64_t addr = bufaddr + line_counter * MAX_ELF_FILE_WIDTH * sizeof(char);
            char *linebuf = (char*)addr;
            
            int i = 0;
            while(i < len && i < MAX_ELF_FILE_WIDTH)
            {
                if ((line[i] == '\n' || line[i] == '\r') ||
                    ((i + 1) < len && (i + 1) < MAX_ELF_FILE_WIDTH && line[i] == '/' && line[i + 1] == '/'))
                {
                    break;
                }
                linebuf[i]  = line[i];
                i ++;
            }
             
            linebuf[i] = '\0';
            line_counter++;
        }
        else
        {
            debug_printf(DEBUG_LINKER, "ELF FILE %s is too long", filename);
            exit(1);
        }
    }

    fclose(fp);

    assert(string2uint((char*)bufaddr) == line_counter);

    return line_counter;
}

void parse_elf(char *filename, elf_t *elf)
{
    assert(elf != NULL);

    int line_count = read_elf(filename, (uint64_t)(&(elf->buffer)));
    elf->line_count = line_count;
    for(int i = 0; i < line_count; i++)
    {
        printf("[%d]\t%s\n", i, elf->buffer[i]);
    }

    int sh_count = string2uint(elf->buffer[1]);

    elf->sht = malloc(sh_count *sizeof(sh_entry_t));

    sh_entry_t *symtab_sh= NULL;
    for (int i = 0; i < sh_count; i++)
    {
        parse_sh(elf->buffer[2 + i], &(elf->sht[i]));
        print_sh_entry(&(elf->sht[i]));

        if (strcmp(elf->sht[i].sh_name, ".symtab") == 0)
        {
            symtab_sh = &(elf->sht[i]);
        }
    }

    assert(symtab_sh != NULL);

    elf->symtab_count = symtab_sh->sh_size;
    elf->symtab = malloc(elf->symtab_count * sizeof(st_entry_t));
    for (int i = 0; i < elf->symtab_count; i++)
    {
        parse_symtab(elf->buffer[i + symtab_sh->sh_offset], &(elf->symtab[i]));

        print_symtab_entry(&(elf->symtab[i]));
    }

}

void free_elf(elf_t *elf)
{
    assert(elf != NULL);

    free(elf->sht);
    free(elf->symtab);
}