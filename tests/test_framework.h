/* test_framework.h — Custom test framework (zero external deps) */
#ifndef M68K_TEST_FRAMEWORK_H
#define M68K_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Minimalist xUnit-style test framework.
 *
 * Usage:
 *   TEST(myTestName) {
 *       ASSERT_EQ(2 + 2, 4);
 *       ASSERT_TRUE(someCondition);
 *   }
 *
 *   int main(void) {
 *       RUN_TEST(myTestName);
 *       TEST_REPORT();
 *       return TEST_EXIT_CODE();
 *   }
 */

/* Counters */
static int tf_total   = 0;
static int tf_passed  = 0;
static int tf_failed  = 0;
static int tf_curFail = 0;

/* Colors */
#define TF_RED   "\033[31m"
#define TF_GREEN "\033[32m"
#define TF_RESET "\033[0m"

/* Define a test function */
#define TEST(name) static void test_##name(void)

/* Run a test */
#define RUN_TEST(name) do {                                                   \
    tf_total++;                                                               \
    tf_curFail = 0;                                                           \
    test_##name();                                                            \
    if (tf_curFail == 0) {                                                    \
        tf_passed++;                                                          \
        printf(TF_GREEN "  PASS" TF_RESET " %s\n", #name);                   \
    } else {                                                                  \
        tf_failed++;                                                          \
        printf(TF_RED "  FAIL" TF_RESET " %s\n", #name);                     \
    }                                                                         \
} while (0)

/* Grouped test suite */
#define TEST_SUITE(name)  static void suite_##name(void)
#define RUN_SUITE(name)   do { printf("\n=== %s ===\n", #name); suite_##name(); } while (0)

/* Summary */
#define TEST_REPORT() do {                                                    \
    printf("\n----- Results -----\n");                                        \
    printf("Total:  %d\n", tf_total);                                         \
    printf("Passed: " TF_GREEN "%d" TF_RESET "\n", tf_passed);               \
    printf("Failed: %s%d" TF_RESET "\n",                                      \
           tf_failed > 0 ? TF_RED : TF_GREEN, tf_failed);                    \
} while (0)

#define TEST_EXIT_CODE()  (tf_failed > 0 ? 1 : 0)

/* ── Assertions ─────────────────────────────────────── */

#define TF_FAIL(msg) do {                                                     \
    tf_curFail++;                                                             \
    fprintf(stderr, "    ASSERT FAILED at %s:%d: %s\n",                       \
            __FILE__, __LINE__, msg);                                          \
} while (0)

#define ASSERT_TRUE(cond) do {                                                \
    if (!(cond)) TF_FAIL(#cond " is not true");                               \
} while (0)

#define ASSERT_FALSE(cond) do {                                               \
    if ((cond)) TF_FAIL(#cond " is not false");                               \
} while (0)

#define ASSERT_EQ(a, b) do {                                                  \
    long long _a = (long long)(a), _b = (long long)(b);                       \
    if (_a != _b) {                                                           \
        char _buf[128];                                                       \
        snprintf(_buf, sizeof(_buf), "%s == %s  (%lld != %lld)", #a, #b, _a, _b); \
        TF_FAIL(_buf);                                                        \
    }                                                                         \
} while (0)

#define ASSERT_NEQ(a, b) do {                                                 \
    long long _a = (long long)(a), _b = (long long)(b);                       \
    if (_a == _b) {                                                           \
        char _buf[128];                                                       \
        snprintf(_buf, sizeof(_buf), "%s != %s  (both %lld)", #a, #b, _a);   \
        TF_FAIL(_buf);                                                        \
    }                                                                         \
} while (0)

#define ASSERT_STR_EQ(a, b) do {                                              \
    if (strcmp((a), (b)) != 0) {                                              \
        char _buf[256];                                                       \
        snprintf(_buf, sizeof(_buf), "\"%s\" != \"%s\"", (a), (b));           \
        TF_FAIL(_buf);                                                        \
    }                                                                         \
} while (0)

#define ASSERT_NULL(ptr)    ASSERT_TRUE((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != NULL)

#define ASSERT_MEM_EQ(a, b, n) do {                                           \
    if (memcmp((a), (b), (n)) != 0)                                           \
        TF_FAIL("memory regions differ");                                     \
} while (0)

/* Hex comparison for registers / addresses */
#define ASSERT_HEX_EQ(a, b) do {                                              \
    unsigned long long _a = (unsigned long long)(a);                           \
    unsigned long long _b = (unsigned long long)(b);                           \
    if (_a != _b) {                                                           \
        char _buf[128];                                                       \
        snprintf(_buf, sizeof(_buf), "%s == %s  (0x%llX != 0x%llX)", #a, #b, _a, _b); \
        TF_FAIL(_buf);                                                        \
    }                                                                         \
} while (0)

#endif /* M68K_TEST_FRAMEWORK_H */
