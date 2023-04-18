#include<headers/cpu.h>
#include<headers/memory.h>
#include<headers/common.h>

uint64_t read64bits_dram(uint64_t paddr, core_t *cr)
{
    if (DEBUG_ENABLE_SRAM_CACHE == 1)
    {
        return 0x0;
    }

    uint64_t val = 0x0;

    val += (((uint64_t)pm[paddr + 0]) << 0);
    val += (((uint64_t)pm[paddr + 1]) << 8);
    val += (((uint64_t)pm[paddr + 2]) << 16);
    val += (((uint64_t)pm[paddr + 3]) << 24);
    val += (((uint64_t)pm[paddr + 4]) << 32);
    val += (((uint64_t)pm[paddr + 5]) << 40);
    val += (((uint64_t)pm[paddr + 6]) << 48);
    val += (((uint64_t)pm[paddr + 7]) << 56);

    return val;
}
void write64bits_dram(uint64_t paddr, uint64_t data, core_t *cr)
{
    if (DEBUG_ENABLE_SRAM_CACHE == 1)
    {
        return;
    }
    // little-endian
    pm[paddr + 0] = (data >> 0) & 0xff;
    pm[paddr + 1] = (data >> 8) & 0xff;
    pm[paddr + 2] = (data >> 16) & 0xff;
    pm[paddr + 3] = (data >> 24) & 0xff;
    pm[paddr + 4] = (data >> 32) & 0xff;
    pm[paddr + 5] = (data >> 40) & 0xff;
    pm[paddr + 6] = (data >> 48) & 0xff;
    pm[paddr + 7] = (data >> 56) & 0xff;
}
