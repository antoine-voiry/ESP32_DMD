# Host tests. make -C test/host            core tests (no dependencies)
#             make -C test/host all        + JSON and firmware tests (fetches the libraries once)
#             make -C test/host coverage   all tests with line coverage, fails under 90 %
CXX ?= g++
CC ?= gcc
SRC_DIR := ../../src
BUILD := build
DEPS := deps
WARN := -Wall -Wextra -Werror
SAN := -fsanitize=address,undefined
CXXFLAGS ?= -std=gnu++17 -O1 -g

CORE_SRCS := $(wildcard $(SRC_DIR)/core/*.cpp)
FW_SRCS := $(CORE_SRCS) $(wildcard $(SRC_DIR)/net/*.cpp $(SRC_DIR)/util/*.cpp $(SRC_DIR)/render/*.cpp \
           $(SRC_DIR)/matrix/*.cpp) $(SRC_DIR)/main.cpp fakes/fakes.cpp

# Same versions as platformio.ini resolves.
ARDUINOJSON := $(DEPS)/ArduinoJson/src
GFX := $(DEPS)/Adafruit-GFX-Library
GIF := $(DEPS)/AnimatedGIF/src
PNGDEC := $(DEPS)/PNGdec/src
LIB_INCLUDES := -isystem $(ARDUINOJSON) -isystem $(GFX) -isystem $(GIF) -isystem $(PNGDEC)
FW_DEFINES := -DARDUINO=10819 -D__LINUX__
FW_INCLUDES := -Ifakes -I$(SRC_DIR) $(LIB_INCLUDES)
ZLIB_SRCS := $(addprefix $(PNGDEC)/,adler32.c crc32.c infback.c inffast.c inflate.c inftrees.c zutil.c)

.PHONY: run all core cxx11 json firmware coverage deps clean
run: core

all: core json firmware

core: cxx11 $(BUILD)/test_core
	./$(BUILD)/test_core

# The ESP32 toolchain is GCC 8; also make sure src/core still compiles as C++11.
cxx11:
	$(CXX) -std=gnu++11 $(WARN) -fsyntax-only -I$(SRC_DIR) $(CORE_SRCS)

json: $(BUILD)/test_json
	./$(BUILD)/test_json

firmware: $(BUILD)/test_firmware
	./$(BUILD)/test_firmware

$(BUILD)/test_core: test_core.cpp check.h $(CORE_SRCS) $(wildcard $(SRC_DIR)/core/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(WARN) $(SAN) -I$(SRC_DIR) test_core.cpp $(CORE_SRCS) -o $@

# -Wno-error=maybe-uninitialized: GCC reports a known false positive inside ArduinoJson once inlined.
$(BUILD)/test_json: test_json.cpp check.h $(SRC_DIR)/net/OnlineJson.cpp $(CORE_SRCS) | $(DEPS)/.done
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(WARN) $(SAN) -Wno-error=maybe-uninitialized -I$(SRC_DIR) -isystem $(ARDUINOJSON) \
		test_json.cpp $(SRC_DIR)/net/OnlineJson.cpp $(CORE_SRCS) -o $@

# Third-party libraries: built once, without our warning flags.
$(BUILD)/libs.a: | $(DEPS)/.done
	@mkdir -p $(BUILD)/libs
	cd $(BUILD)/libs && $(CXX) $(CXXFLAGS) $(SAN) -fno-sanitize=alignment -w $(FW_DEFINES) -I../../fakes $(addprefix -isystem ../../,$(ARDUINOJSON) $(GFX) $(GIF) $(PNGDEC)) \
		-c ../../$(GFX)/Adafruit_GFX.cpp ../../$(PNGDEC)/PNGdec.cpp
	# openFLASH() uses a reader that gif.inl only defines off Linux.
	cd $(BUILD)/libs && $(CXX) $(CXXFLAGS) $(SAN) -fno-sanitize=alignment -w $(FW_DEFINES) -DreadFLASH=readMem -I../../fakes \
		-c ../../$(GIF)/AnimatedGIF.cpp
	cd $(BUILD)/libs && $(CC) -O1 -g $(SAN) -fno-sanitize=alignment -w -D__LINUX__ -c $(addprefix ../../,$(ZLIB_SRCS))
	ar rcs $@ $(BUILD)/libs/*.o

FW_TESTS := test_firmware.cpp $(wildcard fw_*.cpp)

$(BUILD)/test_firmware: $(FW_TESTS) fw_test.h fixtures.h check.h $(FW_SRCS) $(wildcard fakes/*.h fakes/*/*.h $(SRC_DIR)/*/*.h) $(BUILD)/libs.a
	$(CXX) $(CXXFLAGS) $(WARN) $(SAN) -Wno-error=maybe-uninitialized $(FW_DEFINES) $(FW_INCLUDES) \
		$(FW_TESTS) $(FW_SRCS) $(BUILD)/libs.a -Wl,--wrap=time -o $@

# Coverage of everything under src/, from the three test programs.
coverage: | $(DEPS)/.done $(BUILD)/libs.a
	rm -rf $(BUILD)/cov && mkdir -p $(BUILD)/cov
	cd $(BUILD)/cov && $(CXX) -std=gnu++17 -O0 -g --coverage -I../../$(SRC_DIR) \
		../../test_core.cpp $(addprefix ../../,$(CORE_SRCS)) -o test_core && ./test_core
	cd $(BUILD)/cov && $(CXX) -std=gnu++17 -O0 -g --coverage -I../../$(SRC_DIR) -isystem ../../$(ARDUINOJSON) \
		../../test_json.cpp $(addprefix ../../,$(SRC_DIR)/net/OnlineJson.cpp $(CORE_SRCS)) -o test_json && ./test_json
	cd $(BUILD)/cov && $(CXX) -std=gnu++17 -O0 -g --coverage $(SAN) $(FW_DEFINES) -I../../fakes -I../../$(SRC_DIR) \
		$(addprefix -isystem ../../,$(ARDUINOJSON) $(GFX) $(GIF) $(PNGDEC)) \
		$(addprefix ../../,$(FW_TESTS) $(FW_SRCS)) ../libs.a -Wl,--wrap=time -o test_firmware && ./test_firmware
	gcovr --root ../.. --filter '../../src/' --object-directory $(BUILD)/cov --fail-under-line 90 \
		--print-summary --txt $(BUILD)/coverage.txt $(BUILD)/cov
	@cat $(BUILD)/coverage.txt

deps: $(DEPS)/.done

$(DEPS)/.done:
	@mkdir -p $(DEPS)
	git clone -q --depth 1 --branch v7.4.3 https://github.com/bblanchon/ArduinoJson.git $(DEPS)/ArduinoJson
	git clone -q --depth 1 --branch 1.12.6 https://github.com/adafruit/Adafruit-GFX-Library.git $(DEPS)/Adafruit-GFX-Library
	git clone -q --depth 1 --branch 2.2.0 https://github.com/bitbank2/AnimatedGIF.git $(DEPS)/AnimatedGIF
	git clone -q --depth 1 --branch 1.1.7 https://github.com/bitbank2/PNGdec.git $(DEPS)/PNGdec
	@touch $@

clean:
	rm -rf $(BUILD)
