# ===== 项目配置 =====
TARGET   = main
BUILD    = build

# ===== 工具链 =====
CC       = arm-none-eabi-gcc
OBJCOPY  = arm-none-eabi-objcopy
SIZE     = arm-none-eabi-size
NM       = arm-none-eabi-nm
GDB      = arm-none-eabi-gdb

# ===== 编译选项 =====
MCU      = -mcpu=cortex-m0plus -mthumb
CFLAGS   = $(MCU) -O2 -Wall -Wextra -g3 -ffunction-sections -fdata-sections
LDFLAGS  = $(MCU) -T linker.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

# 关键：显式链接 libgcc，防止将来除法/浮点/64位运算找不到 __aeabi_* 符号
LIBGCC   = $(shell $(CC) $(MCU) -print-libgcc-file-name)

# ===== OpenOCD =====
OCD_IF   = interface/cmsis-dap.cfg
OCD_TGT  = target/stm32l0.cfg
OCD_CMD  = "program $(BUILD)/$(TARGET).elf verify reset exit"

# ===== 源文件与目标文件 =====
SRCS     = main.c startup.c
OBJS     = $(addprefix $(BUILD)/, $(SRCS:.c=.o))

# ===== 目标 =====
.PHONY: all flash clean size bin debug

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin
	@echo "===== Size ====="
	@$(SIZE) $(BUILD)/$(TARGET).elf

# ---- ELF 链接 ----
$(BUILD)/$(TARGET).elf: $(OBJS) linker.ld | $(BUILD)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBGCC) -o $@

# ---- 编译每个 .c 文件到 .o（增量编译，改一个只重编一个） ----
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- 生成 .bin（有些烧录器 / 批量生产用得上） ----
$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# ---- 创建 build 目录 ----
$(BUILD):
	mkdir -p $(BUILD)

# ---- 烧录 ----
flash: $(BUILD)/$(TARGET).elf
	openocd -f $(OCD_IF) -f $(OCD_TGT) -c $(OCD_CMD)

# ---- 调试（起 GDB server 后另开终端连） ----
debug:
	openocd -f $(OCD_IF) -f $(OCD_TGT)

# ---- 显示占用 ----
size: $(BUILD)/$(TARGET).elf
	@$(SIZE) $<
	@echo "--- 段布局 (看 .data 的 LMA 应在 0x0800xxxx) ---"
	arm-none-eabi-objdump -h $< | grep -E "\.text|\.data|\.bss|\.isr|\.rodata"

# ---- 检查是否有未定义符号 ----
check-symbols: $(BUILD)/$(TARGET).elf
	@echo "--- 未定义符号（应为空）---"
	@$(NM) $< | grep " U " || echo "(无未定义符号)"

# ---- 清理 ----
clean:
	rm -rf $(BUILD)
	rm -f *.elf *.bin *.hex *.map

# ---- 自动依赖（可选，让头文件改动触发重编） ----
-include $(OBJS:.o=.d)