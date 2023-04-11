#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define PHYSICAL_MEMORY_SPACE 65536
#define MAX_INDEX_PHYSICAL_PAGE 16

uint8_t pm[PHYSICAL_MEMORY_SPACE];

uint64_t read64bits_dram(uint64_t paddr, core_t *cr);

void write64bits_dram(uint64_t paddr, uint64_t data, core_t *cr);

#endif