BUILD := build
TARGET := $(BUILD)/gicv2-lab

# Set LLVM_BIN=/path/to/llvm/bin when LLVM tools are not on PATH.
LLVM_BIN ?=
CLANG := $(if $(LLVM_BIN),$(LLVM_BIN)/)clang
OBJCOPY := $(if $(LLVM_BIN),$(LLVM_BIN)/)llvm-objcopy
OBJDUMP := $(if $(LLVM_BIN),$(LLVM_BIN)/)llvm-objdump

QEMU ?= ../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64
SMOKE_RUNS ?= 100

CPPFLAGS := -Iinclude
COMMON_FLAGS := --target=aarch64-none-elf -mcpu=cortex-a72 \
	-ffreestanding -fno-builtin -fno-pic -fno-stack-protector \
	-mgeneral-regs-only -ffunction-sections -fdata-sections \
	-Wall -Wextra -Werror -O2 -g
CFLAGS := $(COMMON_FLAGS) -std=c11
ASFLAGS := $(COMMON_FLAGS)
LDFLAGS := --target=aarch64-none-elf -mcpu=cortex-a72 -nostdlib -fuse-ld=lld \
	-Wl,-T,linker.ld -Wl,-Map,$(TARGET).map -Wl,--build-id=none \
	-Wl,--gc-sections

C_SOURCES := src/exception.c src/main.c src/print.c src/stage2.c src/uart.c
ASM_SOURCES := arch/arm64/guest.S arch/arm64/start.S arch/arm64/vectors.S
OBJECTS := $(C_SOURCES:%.c=$(BUILD)/%.o) \
	$(ASM_SOURCES:%.S=$(BUILD)/%.o)

.PHONY: all clean smoke smoke-repeat disassembly

all: $(TARGET).elf $(TARGET).bin disassembly

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CLANG) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CLANG) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) linker.ld
	@mkdir -p $(dir $@)
	$(CLANG) $(LDFLAGS) $(OBJECTS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

disassembly: $(TARGET).elf
	$(OBJDUMP) -d $< > $(TARGET).dis

smoke: all
	QEMU="$(QEMU)" scripts/smoke-qemu.sh "$(TARGET).elf"

smoke-repeat: all
	@run=1; \
	while test $$run -le $(SMOKE_RUNS); do \
		SMOKE_QUIET=1 QEMU="$(QEMU)" \
			scripts/smoke-qemu.sh "$(TARGET).elf" || exit 1; \
		run=$$((run + 1)); \
	done; \
	echo "gicv2-lab: $(SMOKE_RUNS) consecutive H2 smoke runs passed"

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
