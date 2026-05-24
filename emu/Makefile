# HerOS RISC-V Emulator — Makefile
# ─────────────────────────────────────────────────────────────────

# Host compiler
CC        = gcc
CFLAGS    = -O2 -Wall -Wextra -g $(shell pkg-config --cflags sdl2)
LDFLAGS   = $(shell pkg-config --libs sdl2)

# RISC-V cross compiler
RV_CC     = riscv64-unknown-elf-gcc
RV_OBJCOPY = riscv64-unknown-elf-objcopy
RV_CFLAGS  = -march=rv32i_zicsr -mabi=ilp32 -nostdlib -nostartfiles -ffreestanding -O2 -Wall
RV_LDFLAGS = -march=rv32i_zicsr -mabi=ilp32 -nostdlib -T test/linker.ld

# ─── Emulator ───────────────────────────────────────────────────
EMU_SRC = src/main.c src/cpu.c src/bus.c src/uart.c src/clint.c src/disk.c src/fb.c
EMU_BIN = heros-emu

# ─── Test kernel ────────────────────────────────────────────────
KERN_SRC  = test/boot.S test/kernel.c
KERN_ELF  = test/kernel.elf
KERN_BIN  = test/kernel.bin

# ─── Targets ────────────────────────────────────────────────────

all: $(EMU_BIN) $(KERN_BIN)

$(EMU_BIN): $(EMU_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(KERN_ELF): $(KERN_SRC) test/linker.ld
	$(RV_CC) $(RV_CFLAGS) $(RV_LDFLAGS) -o $@ $(KERN_SRC)

$(KERN_BIN): $(KERN_ELF)
	$(RV_OBJCOPY) -O binary $< $@

run: all
	./$(EMU_BIN) $(KERN_BIN)

clean:
	rm -f $(EMU_BIN) $(KERN_ELF) $(KERN_BIN)

.PHONY: all run clean
