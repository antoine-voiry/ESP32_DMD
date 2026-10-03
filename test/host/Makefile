# Host-side unit tests for the hardware-independent code in src/core.
# Usage: make -C test/host
CXX ?= g++
CXXFLAGS ?= -std=gnu++17 -Wall -Wextra -Werror -O1 -g
SRC_DIR := ../../src
CORE_SRCS := $(wildcard $(SRC_DIR)/core/*.cpp)
BUILD := build

all: run

$(BUILD)/test_core: test_core.cpp $(CORE_SRCS) $(wildcard $(SRC_DIR)/core/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -I$(SRC_DIR) test_core.cpp $(CORE_SRCS) -o $@

run: $(BUILD)/test_core
	./$(BUILD)/test_core

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
