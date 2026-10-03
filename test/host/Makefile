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

# JSON parsers (src/net) need ArduinoJson: make json ARDUINOJSON=/path/to/ArduinoJson/src
json: $(BUILD)/test_json
	./$(BUILD)/test_json

$(BUILD)/test_json: test_json.cpp $(SRC_DIR)/net/OnlineJson.cpp $(CORE_SRCS)
	@test -n "$(ARDUINOJSON)" || (echo "Set ARDUINOJSON=<ArduinoJson>/src" && false)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -I$(SRC_DIR) -I$(ARDUINOJSON) test_json.cpp $(SRC_DIR)/net/OnlineJson.cpp $(CORE_SRCS) -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all run clean cxx11 json
