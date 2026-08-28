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

# lwIP (manifest 3) - enabled by default; build without via `make LWIP=0`.
# OS mode (netconn/sockets + coreMQTT) via `make LWIP_OS=1`.
LWIP ?= 1
LWIP_OS ?= 0
LWIP_DEFS :=
LWIP_BUILD_SRCS :=
ifeq ($(LWIP),1)
include port/lwip/lwip.mk
INCLUDES += $(LWIP_INC) -Iport/lwip/netif
LWIP_BUILD_SRCS := $(LWIP_SRCS)
ifeq ($(LWIP_OS),1)
INCLUDES += $(MQTT_INC) $(PTP_INC)
LWIP_DEFS := -DFLINT_LWIP_OS
LWIP_BUILD_SRCS += $(LWIP_API) $(MQTT_SRCS) $(PTP_SRCS) demo/rpi4/net_demo_os.c
else
LWIP_BUILD_SRCS += demo/rpi4/net_demo.c
endif
endif

CFLAGS   := --target=$(TARGET) -mcpu=$(CPU) -ffreestanding -nostdlib \
            -mgeneral-regs-only -fno-stack-protector -fno-common -fno-builtin \
            -O2 -Wall -Wextra -Wshadow -std=c11 -DconfigUSE_LWIP=$(LWIP) $(LWIP_DEFS) $(INCLUDES)
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

ifeq ($(LWIP),1)
C_SRCS += $(LWIP_BUILD_SRCS)
endif

S_SRCS := \
    boot/rpi4/start.S \
    portable/LLVM_AArch64/vectors.S \
    portable/LLVM_AArch64/portASM.S

OBJS := $(patsubst %,$(BUILD)/%.o,$(C_SRCS) $(S_SRCS))

.PHONY: all clean qemu disasm
all: $(IMG)

# Vendored third-party code (lwIP): suppress warnings, keep our code strict.
$(BUILD)/third_party/%.c.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CLANG) $(CFLAGS) -w -c $< -o $@

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
	rm -rf $(BUILD)
	       $(BUILD)/kernel $(ELF) $(IMG)
