DIRS := src drivers nonfree utils
HDR := $(wildcard $(addsuffix /*.h,$(DIRS)))
SRC := $(wildcard $(addsuffix /*.c,$(DIRS)))
OBJ := $(patsubst %.c,build/%.o,$(SRC))

CFLAGS = \
	 -Idrivers -Inonfree -Isrc -Iutils \
	 -D__int64_t_defined=1 \
	 -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wundef \
	 -Wmissing-prototypes -Wpointer-arith -Wfloat-equal \
	 -g3 -Os -MMD -MP \
	 -mcpu=cortex-a7 -march=armv7ve \
	 -mfpu=neon-vfpv4 -mfloat-abi=hard -mlittle-endian \
	 -fdata-sections -ffunction-sections \
	 -ffreestanding -fno-common -nostartfiles \

LFLAGS = \
	 -Wl,--gc-sections \
	 -Wl,-Map,build/main.map,--cref \
	 -mcpu=cortex-a7 -march=armv7ve -mfpu=neon-vfpv4 \
	 -mlittle-endian -mfloat-abi=hard \
	 -specs=nano.specs -specs=nosys.specs \
	 -nostartfiles \
	 -ffreestanding \
	 -Wl,--print-memory-usage \

# Build recipes

build/sdcard.img: build/main.stm32 build/blink.bin
	python3 scripts/sdimage.py $@ $^

build/main.elf: $(OBJ) build/src/handoff.o
	arm-none-eabi-gcc $(LFLAGS) -T src/sysram.ld -o $@ $^

build/blink.elf: build/utils/printf.o build/test/startup.o build/test/blink.o
	arm-none-eabi-gcc $(LFLAGS) -T test/blink.ld -o $@ $^

build/%.o: %.c
	mkdir -p $(dir $@)
	arm-none-eabi-gcc -c $(CPPFLAGS) $(CFLAGS) $(CFLAGS_EXTRA) $< -o $@

build/%.o: %.S
	mkdir -p $(dir $@)
	arm-none-eabi-gcc -c $(CPPFLAGS) $(CFLAGS) $(CFLAGS_EXTRA) $< -o $@

%.bin: %.elf
	arm-none-eabi-objcopy -O binary $< $@

%.stm32: %.bin
	python3 scripts/stm32_header.py -e $*.elf -b $< -o $@ -t .RESET

# Static code analysis

check: format cppcheck tidy inclusions done

format:
	clang-format --dry-run -Werror $(wildcard */*.[ch])

tidy:
	make clean
	mkdir -p build
	bear --output build/compile_commands.json -- make \
		 -j$(shell nproc) $(TARGET)
	run-clang-tidy -j$(shell nproc) -p build $(wildcard src/*.c) \
		-extra-arg=-I/usr/lib/arm-none-eabi/include

cppcheck:
	cppcheck --enable=all --inconclusive --std=c99 --force --quiet \
	--inline-suppr --error-exitcode=1 --suppress=missingInclude src util

inclusions: $(INC_SRCS) scripts/inclusions.py
	python3 scripts/inclusions.py $(wildcard src/*.[ch]) > build/incl.dot; \
	PYTHON_EXIT=$$? ; \
	dot -Tpdf build/incl.dot -o build/incl.pdf ; \
	exit $$PYTHON_EXIT

done:
	@echo "\033[1;32mSUCCESS\033[0m: All checks passed."

# General

.PHONY: clean check format tidy cppcheck inclusions term install destroy

clean:
	rm -rf build

term:
	python3 -m serial.tools.miniterm $(PORT) 115200

install: build/main.stm32
	python3 scripts/uart_boot.py -c $(PORT) -f $<

destroy: build/sdcard.img
	dd if=build/sdcard.img of=$(DRIVE)

-include $(OBJ:.o=.d)
