BUILD := build
TARGET := $(BUILD)/gicv2-lab

# Set LLVM_BIN=/path/to/llvm/bin when LLVM tools are not on PATH.
LLVM_BIN ?=
CLANG := $(if $(LLVM_BIN),$(LLVM_BIN)/)clang
OBJCOPY := $(if $(LLVM_BIN),$(LLVM_BIN)/)llvm-objcopy
OBJDUMP := $(if $(LLVM_BIN),$(LLVM_BIN)/)llvm-objdump
PYTHON ?= python3

QEMU ?= ../qemu-rpi4/qemu-pi4/build-pi4-native-fdt/qemu-system-aarch64
SMOKE_RUNS ?= 100
H5_SCENARIO ?= scenarios/h4i-context-save-restore-v1.json
H5_OUT ?= $(BUILD)/h5-qemu
H5_REPEAT_OUT ?= $(BUILD)/h5-repeat
H5_RUNS ?= 10
H7_BUILD ?= $(BUILD)/h7
H7_OUT ?= $(BUILD)/h7-qemu
H7_RUNS ?= 20

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

C_SOURCES := src/exception.c src/gicv2.c src/main.c src/print.c src/stage2.c \
	src/uart.c src/watchdog.c
ASM_SOURCES := arch/arm64/guest.S arch/arm64/start.S arch/arm64/vectors.S
OBJECTS := $(C_SOURCES:%.c=$(BUILD)/%.o) \
	$(ASM_SOURCES:%.S=$(BUILD)/%.o)

.PHONY: all clean smoke smoke-repeat disassembly h5-test h6-test h7-test h5-qemu h5-repeat h7-build h7-qemu

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
	echo "gicv2-lab: $(SMOKE_RUNS) consecutive H4i smoke runs passed"

h5-test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tests/test_h5_runner.py

h6-test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tests/test_h6_gate.py

h7-test:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tests/test_h7_runner.py

# H7 bundles are immutable: select a fresh H7_BUILD for a new build.
h7-build:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tools/h7_runner.py build \
		$(if $(LLVM_BIN),--llvm-bin "$(LLVM_BIN)",) --out "$(H7_BUILD)"

h7-qemu:
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tools/h7_runner.py capture-qemu \
		--bundle "$(H7_BUILD)" --out "$(H7_OUT)" --qemu "$(QEMU)" --runs "$(H7_RUNS)"

h5-qemu: all
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tools/h5_runner.py capture-qemu \
		--scenario "$(H5_SCENARIO)" --image "$(TARGET).elf" --qemu "$(QEMU)" \
		--out "$(H5_OUT)"

h5-repeat: all
	PYTHONDONTWRITEBYTECODE=1 $(PYTHON) tools/h5_runner.py repeat-qemu \
		--scenario "$(H5_SCENARIO)" --image "$(TARGET).elf" --qemu "$(QEMU)" \
		--out "$(H5_REPEAT_OUT)" --runs "$(H5_RUNS)"

clean:
	rm -rf $(BUILD)

-include $(OBJECTS:.o=.d)
