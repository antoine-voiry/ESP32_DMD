// Host tests for the firmware services, scenes, web pages and main.cpp, on fakes of the ESP32
// libraries (test/host/fakes). Run: make -C test/host firmware
#include "fw_test.h"

TEST_MAIN_COUNTERS;

int main() {
    testServices();
    testRendering();
    testOnline();
    testMessages();
    testWeb();
    testMain();  // last: setup() runs once per process
    return TEST_REPORT();
}
