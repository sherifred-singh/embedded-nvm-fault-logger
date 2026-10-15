/**
 * @file unity.c
 * @brief Lightweight Embedded Unit Test Framework Implementation
 * @author Sherifred Singh
 */

#include "unity.h"

unity_state_t Unity;

void unity_begin(const char *suite_name)
{
    Unity.tests_run           = 0;
    Unity.tests_passed        = 0;
    Unity.tests_failed        = 0;
    Unity.current_test_failed = 0;

    printf("\n======================================================\n");
    printf(" SUITE: %s\n", suite_name);
    printf("======================================================\n");
}

int unity_end(void)
{
    printf("------------------------------------------------------\n");
    printf(" Total Assertions: %u | Passed: %u | Failed: %u\n",
           Unity.tests_run, Unity.tests_passed, Unity.tests_failed);
    printf(" Status: %s\n", (Unity.tests_failed == 0) ? "ALL TESTS PASSED [OK]" : "TESTS FAILED [ERROR]");
    printf("======================================================\n\n");
    return (int)Unity.tests_failed;
}
