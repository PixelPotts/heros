# HerOS RISC-V — Unified Makefile
# Builds: emulator (host), kernel+hal+ui (rv32im), disk image, tools
# ─────────────────────────────────────────────────────────────────

# Host compiler
CC        = gcc
CFLAGS    = -O2 -Wall -Wextra -g $(shell pkg-config --cflags sdl2)
LDFLAGS   = $(shell pkg-config --libs sdl2)

# RISC-V cross compiler
RV_CC      = riscv64-unknown-elf-gcc
RV_OBJCOPY = riscv64-unknown-elf-objcopy
RV_MARCH   = rv32im_zicsr
RV_MABI    = ilp32
RV_CFLAGS  = -march=$(RV_MARCH) -mabi=$(RV_MABI) \
             -nostdlib -nostartfiles -ffreestanding \
             -O2 -Wall -Wextra \
             -Ikernel -Ihal -Iui
RV_LDFLAGS = -march=$(RV_MARCH) -mabi=$(RV_MABI) \
             -nostdlib -T kernel/linker.ld

# ─── Emulator ───────────────────────────────────────────────────
EMU_SRC = src/main.c src/cpu.c src/bus.c src/uart.c src/clint.c \
          src/disk.c src/input.c src/fb.c
EMU_BIN = heros-emu

# ─── Kernel sources ─────────────────────────────────────────────
KERN_SRC = kernel/boot.S \
           kernel/kmain.c \
           kernel/string.c \
           kernel/kprintf.c \
           kernel/mm.c \
           kernel/trap.c \
           kernel/timer.c \
           kernel/uart.c \
           kernel/sched.c \
           kernel/syscall.c \
           kernel/disk.c \
           kernel/input.c \
           kernel/fb.c \
           kernel/fs.c

# ─── HAL sources ────────────────────────────────────────────────
HAL_SRC = hal/hal_fb.c \
          hal/hal_draw.c \
          hal/hal_text.c \
          hal/hal_input.c \
          hal/hal_time.c \
          hal/hal_mem.c \
          hal/hal_fs.c \
          hal/font_data.c

# ─── UI sources ─────────────────────────────────────────────────
UI_SRC  = ui/theme.c \
          ui/animation.c \
          ui/draw.c \
          ui/icons.c \
          ui/widgets.c \
          ui/window.c \
          ui/wm.c \
          ui/topbar.c \
          ui/sidebar.c \
          ui/dock.c \
          ui/systray.c \
          ui/lockscreen.c \
          ui/desktop.c \
          ui/app_registry.c \
          ui/apps/terminal.c \
          ui/apps/filemanager.c \
          ui/apps/settings.c \
          ui/apps/taskmanager.c

ALL_RV_SRC = $(KERN_SRC) $(HAL_SRC) $(UI_SRC)

KERN_ELF = kernel.elf
KERN_BIN = kernel.bin

# ─── Disk image ─────────────────────────────────────────────────
DISK_IMG = disk.img
MKFAT16  = tools/mkfat16

# ─── Old test kernel ────────────────────────────────────────────
TEST_SRC = test/boot.S test/kernel.c
TEST_ELF = test/kernel.elf
TEST_BIN = test/kernel.bin

# ─── Targets ────────────────────────────────────────────────────

all: $(EMU_BIN) $(KERN_BIN) $(DISK_IMG)

# Emulator (host binary)
$(EMU_BIN): $(EMU_SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(EMU_SRC) $(LDFLAGS)

# HerOS kernel (RISC-V binary)
$(KERN_ELF): $(ALL_RV_SRC) kernel/linker.ld $(wildcard kernel/*.h hal/*.h ui/*.h ui/apps/*.h)
	$(RV_CC) $(RV_CFLAGS) $(RV_LDFLAGS) -o $@ $(ALL_RV_SRC)

$(KERN_BIN): $(KERN_ELF)
	$(RV_OBJCOPY) -O binary $< $@
	@echo "Kernel: $$(wc -c < $@) bytes"

# mkfat16 tool
$(MKFAT16): tools/mkfat16.c
	$(CC) -O2 -Wall -o $@ $<

# Disk image
$(DISK_IMG): $(MKFAT16)
	./$(MKFAT16) $@ 8

# Old test kernel (for testing emulator changes independently)
test: $(EMU_BIN) $(TEST_BIN)

$(TEST_ELF): $(TEST_SRC) test/linker.ld
	$(RV_CC) -march=rv32i_zicsr -mabi=ilp32 -nostdlib -nostartfiles -ffreestanding -O2 -Wall \
		-march=rv32i_zicsr -mabi=ilp32 -nostdlib -T test/linker.ld -o $@ $(TEST_SRC)

$(TEST_BIN): $(TEST_ELF)
	$(RV_OBJCOPY) -O binary $< $@

# Run
run: all
	./$(EMU_BIN) $(KERN_BIN) $(DISK_IMG)

run-test: test
	./$(EMU_BIN) $(TEST_BIN)

clean:
	rm -f $(EMU_BIN) $(KERN_ELF) $(KERN_BIN) $(TEST_ELF) $(TEST_BIN) $(MKFAT16) $(DISK_IMG)

.PHONY: all test run run-test clean
