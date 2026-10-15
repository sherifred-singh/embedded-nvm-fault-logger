/**
 * @file unity.h
 * @brief Lightweight Embedded Unit Test Framework Header
 * @author Sherifred Singh
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t tests_run;
    uint32_t tests_passed;
    uint32_t tests_failed;
    uint32_t current_test_failed;
} unity_state_t;

extern unity_state_t Unity;

void unity_begin(const char *suite_name);
int  unity_end(void);

#define TEST_ASSERT(condition) do { \
    Unity.tests_run++; \
    if (!(condition)) { \
        Unity.tests_failed++; \
        Unity.current_test_failed = 1; \
        printf("  [FAIL] %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
    } else { \
        Unity.tests_passed++; \
    } \
} while(0)

#define TEST_ASSERT_TRUE(condition)             TEST_ASSERT((condition))
#define TEST_ASSERT_FALSE(condition)            TEST_ASSERT(!(condition))
#define TEST_ASSERT_EQUAL_UINT(expected, actual) TEST_ASSERT((uint32_t)(expected) == (uint32_t)(actual))
#define TEST_ASSERT_EQUAL_INT(expected, actual)  TEST_ASSERT((int32_t)(expected) == (int32_t)(actual))
#define TEST_ASSERT_NOT_NULL(ptr)               TEST_ASSERT((ptr) != NULL)
#define TEST_ASSERT_NULL(ptr)                   TEST_ASSERT((ptr) == NULL)

#define RUN_TEST(test_func) do { \
    Unity.current_test_failed = 0; \
    printf("  RUNNING: %s ... ", #test_func); \
    test_func(); \
    if (!Unity.current_test_failed) { \
        printf("PASS\n"); \
    } \
} while(0)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
