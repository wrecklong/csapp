#include <headers/linker.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <headers/common.h>


//int read_elf(const char *filename, uint64_t bufaddr);

int main()
{
    elf_t srcp[2];
    elf_t *src[2] = {&srcp[0], &srcp[1]};
    parse_elf("./files/exe/sum.elf.txt", src[0]);
    parse_elf("./files/exe/main.elf.txt", src[1]);

    elf_t dst;
    link_elf((elf_t **)&src, 2, &dst);

    free_elf(src[0]);
    free_elf(src[1]);

    return 0;
}