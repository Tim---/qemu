include config.mak
SRC_DIR := $(TOPSRC_DIR)/pc-bios/psp
VPATH = $(SRC_DIR)

all: loader.elf

%.o: %.c
	arm-none-eabi-gcc -c -g -o $@ $^

loader.elf: loader.o utils.o
	arm-none-eabi-gcc -g -o $@ $^ -nostartfiles -Wl,-T -Wl,$(SRC_DIR)/loader.ld

.PHONY: all
