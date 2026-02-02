/* Unity Test Framework - Minimal Header for Embedded Testing
 * A lightweight alternative to gtest for C projects
 * Based on Unity by ThrowTheSwitch.org
 */

#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H

#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Unity Configuration */
#define UNITY_OUTPUT_CHAR(a)    putchar(a)
#define UNITY_OUTPUT_FLUSH()    fflush(stdout)
#define UNITY_OUTPUT_START()    
#define UNITY_OUTPUT_COMPLETE()

/* Test Result Types */
typedef enum
{
    UNITY_DISPLAY_STYLE_INT,
    UNITY_DISPLAY_STYLE_UINT,
    UNITY_DISPLAY_STYLE_HEX8,
    UNITY_DISPLAY_STYLE_HEX16,
    UNITY_DISPLAY_STYLE_HEX32,
} UNITY_DISPLAY_STYLE_T;

/* Internal Unity State */
struct UNITY_STORAGE_T
{
    const char* TestFile;
    const char* CurrentTestName;
    uint32_t CurrentTestLineNumber;
    uint32_t NumberOfTests;
    uint32_t TestFailures;
    uint32_t TestIgnores;
    uint8_t CurrentTestFailed;
    uint8_t CurrentTestIgnored;
    jmp_buf AbortFrame;
};

extern struct UNITY_STORAGE_T Unity;

/* Core Test Functions */
void UnityBegin(const char* filename);
int UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(void (*Func)(void), const char* FuncName, const int FuncLineNum);

/* Assertion Functions */
void UnityAssertEqualNumber(const int32_t expected, const int32_t actual, const char* msg, const uint32_t lineNumber, const UNITY_DISPLAY_STYLE_T style);
void UnityAssertEqualString(const char* expected, const char* actual, const char* msg, const uint32_t lineNumber);
void UnityAssertBits(const int32_t mask, const int32_t expected, const int32_t actual, const char* msg, const uint32_t lineNumber);
void UnityAssertEqualMemory(const void* expected, const void* actual, const uint32_t length, const uint32_t num_elements, const char* msg, const uint32_t lineNumber);
void UnityFail(const char* msg, const uint32_t line);
void UnityIgnore(const char* msg, const uint32_t line);

/* Test Macros */
#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END() UnityEnd()

#define TEST_ASSERT(condition) \
    if (condition) {} else {UnityFail("Expected TRUE Was FALSE", __LINE__);}

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))
#define TEST_ASSERT_NULL(pointer) TEST_ASSERT((pointer) == NULL)
#define TEST_ASSERT_NOT_NULL(pointer) TEST_ASSERT((pointer) != NULL)

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    UnityAssertEqualNumber((int32_t)(expected), (int32_t)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_EQUAL_UINT(expected, actual) \
    UnityAssertEqualNumber((int32_t)(expected), (int32_t)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)

#define TEST_ASSERT_EQUAL_HEX32(expected, actual) \
    UnityAssertEqualNumber((int32_t)(expected), (int32_t)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    UnityAssertEqualString((const char*)(expected), (const char*)(actual), NULL, __LINE__)

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len) \
    UnityAssertEqualMemory((void*)(expected), (void*)(actual), (len), 1, NULL, __LINE__)

#define TEST_FAIL(msg) UnityFail((msg), __LINE__)
#define TEST_IGNORE(msg) UnityIgnore((msg), __LINE__)

/* Test Runner Macros */
#define RUN_TEST(func) UnityDefaultTestRun(func, #func, __LINE__)

/* Test Protection (for exception handling) */
#define TEST_PROTECT() (setjmp(Unity.AbortFrame) == 0)
#define TEST_ABORT() longjmp(Unity.AbortFrame, 1)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_FRAMEWORK_H */
