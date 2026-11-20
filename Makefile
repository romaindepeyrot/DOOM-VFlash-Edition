# SPDX-License-Identifier: GPL-2.0-or-later
CROSS_COMPILE ?= arm-none-eabi-
CC = $(CROSS_COMPILE)gcc
PYTHON ?= python3
BUILD ?= build
ARCH = -mcpu=arm926ej-s -marm -mfloat-abi=soft

CPPFLAGS += -Iengine -Iplatform -DNORMALUNIX -DVFLASH
CFLAGS += $(ARCH) -O2 -g -std=gnu11 -fcommon -fno-short-enums -fsigned-char -fwrapv -fno-strict-aliasing \
          -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS += $(ARCH) -nostartfiles -Wl,-T,platform/vflash.ld,--gc-sections,--no-enum-size-warning \
           -Wl,-Map,$(BUILD)/doom-vflash.map

ENGINE_SRCS := $(wildcard engine/*.c)
PLATFORM_SRCS := $(wildcard platform/*.c) platform/start.S
OBJECTS := $(addprefix $(BUILD)/,$(ENGINE_SRCS:.c=.o) $(PLATFORM_SRCS:.c=.o))
OBJECTS := $(OBJECTS:.S=.o)
ENGINE := $(BUILD)/doom-vflash-engine.elf
GAME := $(BUILD)/doom-vflash.elf

.PHONY: all game clean
all: $(ENGINE)

$(ENGINE): $(OBJECTS) platform/vflash.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) -lm -lc -lgcc

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

game: $(ENGINE)
	$(PYTHON) tools/pack_wad.py "$(ENGINE)" "$(WAD)" "$(GAME)"

clean:
	rm -rf "$(BUILD)"

-include $(OBJECTS:.o=.d)
