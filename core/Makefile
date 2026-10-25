CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
BUILD := build

SRCS := $(wildcard src/*.c) test/test_core.c

.PHONY: test clean

test: $(BUILD)/test_core
	./$(BUILD)/test_core

$(BUILD)/test_core: $(SRCS) $(wildcard include/bb/*.h)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Iinclude $(SRCS) -o $@

clean:
	rm -rf $(BUILD)
