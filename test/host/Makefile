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

# The ESP32 toolchain is GCC 8; also make sure src/core still compiles as C++11.
cxx11:
	$(CXX) -std=gnu++11 -Wall -Wextra -Werror -fsyntax-only -I$(SRC_DIR) $(CORE_SRCS)

run: cxx11 $(BUILD)/test_core
	./$(BUILD)/test_core

clean:
	rm -rf $(BUILD)

.PHONY: all run clean cxx11
