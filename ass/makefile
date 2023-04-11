cc = /usr/bin/gcc-9
CFLAGS = -Wall -g -O2 -Werror -std=gnu99

EXE_HARDWARE = exe_hardware

SRC_DIR = ./src

COMMON = $(SRC_DIR)/common/print.c $(SRC_DIR)/common/convert.c
CPU =$(SRC_DIR)/hardware/cpu/mmu.c $(SRC_DIR)/hardware/cpu/isa.c
MEMORY = $(SRC_DIR)/hardware/memory/dram.c
MAIN_HARDWARE = $(SRC_DIR)/main_hardware.c

.PHONY:hardware
hardware:
		$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(DISK) $(MAIN_HARDWARE) -o $(EXE_HARDWARE)
		./$(EXE_HARDWARE)

clear:
	rm -f *.o  *~ $(EXE)
