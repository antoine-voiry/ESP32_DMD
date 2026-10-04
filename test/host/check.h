#ifndef DMD_TEST_CHECK_H
#define DMD_TEST_CHECK_H

// Minimal assertions shared by the host tests: count, report and keep going.

#include <cstdio>

extern int g_failures;
extern int g_checks;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!((a) == (b))) {                                                     \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);   \
        }                                                                        \
    } while (0)

// Defines the counters; use once per test program.
#define TEST_MAIN_COUNTERS \
    int g_failures = 0;    \
    int g_checks = 0

#define TEST_REPORT()                                                   \
    (std::printf("%d checks, %d failures\n", g_checks, g_failures),    \
     g_failures == 0 ? 0 : 1)

#endif
