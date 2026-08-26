# FlintRTOS - Raspberry Pi 4 (AArch64) bare-metal firmware build.
# Uses the LLVM toolchain (clang cross-compiler + ld.lld + llvm-objcopy);
# no GNU cross-toolchain required.
#
#   make            -> build/kernel8.img  (copy to an SD card, or run in QEMU)
#   make qemu       -> run in qemu-system-aarch64 -M raspi4b (if installed)
#   make clean

# Assigned with := to override make's built-in defaults (e.g. LD=ld).
CLANG    := clang
OBJCOPY  := llvm-objcopy
OBJDUMP  := llvm-objdump
LD       := ld.lld

TARGET   := aarch64-none-elf
CPU      := cortex-a72

BUILD    := build
IMG      := $(BUILD)/kernel8.img
ELF      := $(BUILD)/flint.elf

INCLUDES := -Iinclude -Iinclude/FlintRTOS -Ibsp/rpi4 \
            -Iportable/LLVM_AArch64 -Idemo/rpi4

CFLAGS   := --target=$(TARGET) -mcpu=$(CPU) -ffreestanding -nostdlib \
            -mgeneral-regs-only -fno-stack-protector -fno-common -fno-builtin \
            -O2 -Wall -Wextra -Wshadow -std=c11 $(INCLUDES)
ASFLAGS  := --target=$(TARGET) -mcpu=$(CPU) -ffreestanding $(INCLUDES)
LDFLAGS  := -T boot/rpi4/linker.ld -nostdlib --gc-sections

# Firmware sources.
C_SRCS := \
    boot/rpi4/boot.c \
    bsp/rpi4/uart.c \
    kernel/list.c \
    kernel/tasks.c \
    kernel/queue.c \
    kernel/flint_libc.c \
    portable/LLVM_AArch64/port.c \
    portable/MemMang/heap_4.c \
    demo/rpi4/main.c

S_SRCS := \
    boot/rpi4/start.S \
    portable/LLVM_AArch64/vectors.S \
    portable/LLVM_AArch64/portASM.S

OBJS := $(patsubst %,$(BUILD)/%.o,$(C_SRCS) $(S_SRCS))

.PHONY: all clean qemu disasm
all: $(IMG)

$(BUILD)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CLANG) $(CFLAGS) -c $< -o $@

$(BUILD)/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CLANG) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(IMG): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "built $@ ($$(stat -c %s $@) bytes)"

disasm: $(ELF)
	$(OBJDUMP) -d $(ELF) | head -80

# Requires qemu-system-aarch64 with raspi4b support.
qemu: $(IMG)
	qemu-system-aarch64 -M raspi4b -kernel $(IMG) -serial stdio -display none

clean:
	rm -rf $(BUILD)/boot $(BUILD)/bsp $(BUILD)/demo $(BUILD)/portable \
	       $(BUILD)/kernel $(ELF) $(IMG)
