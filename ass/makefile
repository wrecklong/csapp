cc = /usr/bin/gcc-9
CFLAGS = -Wall -g -O2 -Werror -std=gnu99

EXE_HARDWARE = exe_hardware
EXE_ELF = exe_elf

SRC_DIR = ./src

COMMON = $(SRC_DIR)/common/print.c $(SRC_DIR)/common/convert.c
CPU =$(SRC_DIR)/hardware/cpu/mmu.c $(SRC_DIR)/hardware/cpu/isa.c
MEMORY = $(SRC_DIR)/hardware/memory/dram.c
LINKER = $(SRC_DIR)/LINKER/parseElf.c $(SRC_DIR)/LINKER/staticlink.c
TEST_HARDWARE = $(SRC_DIR)/test/test_hardware.c
TEST_ELF = $(SRC_DIR)/test/test_elf.c


.PHONY:hardware
hardware:
		$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(TEST_HARDWARE) -o $(EXE_HARDWARE)
		./$(EXE_HARDWARE)

.PHONY:link
link:
		$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(LINKER) $(MEMORY) $(TEST_ELF) -o $(EXE_ELF)
		./$(EXE_ELF)


clear:
	rm -f *.o  *~ $(EXE)
