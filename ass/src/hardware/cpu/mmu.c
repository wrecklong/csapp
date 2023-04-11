#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <headers/cpu.h>
#include <headers/common.h>
#include <headers/memory.h>

uint64_t va2pa(uint64_t vaddr)
{
    return vaddr & (0xffffffffffffffff) >> (64 - MAX_INDEX_PHYSICAL_PAGE);
}