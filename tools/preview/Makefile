# Regenerates docs/img from the firmware's own drawing code: make -C tools/preview
CXX ?= g++
SRC_DIR := ../../src
CORE_SRCS := $(wildcard $(SRC_DIR)/core/*.cpp)
BUILD := build
OUT := ../../docs/img

all: $(BUILD)/preview
	rm -rf $(BUILD)/frames && mkdir -p $(BUILD)/frames
	./$(BUILD)/preview $(BUILD)/frames
	python3 make_previews.py $(BUILD)/frames $(OUT)

$(BUILD)/preview: preview.cpp $(CORE_SRCS)
	@mkdir -p $(BUILD)
	$(CXX) -std=gnu++17 -O2 -Wall -Wextra -Werror -I$(SRC_DIR) preview.cpp $(CORE_SRCS) -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
