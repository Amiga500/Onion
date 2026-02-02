/* Unity Test Framework - Implementation
 * Minimal implementation for embedded testing
 */

#include "unity.h"
#include <stdlib.h>

/* Global Unity State */
struct UNITY_STORAGE_T Unity;

/* Initialize Unity Test Framework */
void UnityBegin(const char* filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

/* Finalize Unity Test Framework */
int UnityEnd(void)
{
    printf("\n-----------------------\n");
    printf("%u Tests %u Failures %u Ignored\n", 
           Unity.NumberOfTests, Unity.TestFailures, Unity.TestIgnores);
    
    if (Unity.TestFailures == 0U) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
    
    return (Unity.TestFailures == 0U) ? 0 : 1;
}

/* Conclude current test */
void UnityConcludeTest(void)
{
    if (Unity.CurrentTestIgnored) {
        Unity.TestIgnores++;
        printf("IGNORE\n");
    }
    else if (!Unity.CurrentTestFailed) {
        printf("PASS\n");
    }
    else {
        Unity.TestFailures++;
    }
    
    Unity.NumberOfTests++;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

/* Run a single test */
void UnityDefaultTestRun(void (*Func)(void), const char* FuncName, const int FuncLineNum)
{
    Unity.CurrentTestName = FuncName;
    Unity.CurrentTestLineNumber = (uint32_t)FuncLineNum;
    
    printf("TEST(%s:%u):", Unity.TestFile, Unity.CurrentTestLineNumber);
    
    if (TEST_PROTECT()) {
        Func();
    }
    
    UnityConcludeTest();
}

/* Assert Equal Number */
void UnityAssertEqualNumber(const int32_t expected, const int32_t actual, 
                           const char* msg, const uint32_t lineNumber, 
                           const UNITY_DISPLAY_STYLE_T style)
{
    if (expected != actual) {
        Unity.CurrentTestFailed = 1;
        Unity.CurrentTestLineNumber = lineNumber;
        
        printf("\n%s:%u:", Unity.TestFile, lineNumber);
        if (msg != NULL) {
            printf("%s ", msg);
        }
        
        switch (style) {
            case UNITY_DISPLAY_STYLE_HEX32:
            case UNITY_DISPLAY_STYLE_HEX16:
            case UNITY_DISPLAY_STYLE_HEX8:
                printf("Expected 0x%08X Was 0x%08X", expected, actual);
                break;
            case UNITY_DISPLAY_STYLE_UINT:
                printf("Expected %u Was %u", (unsigned)expected, (unsigned)actual);
                break;
            default:
                printf("Expected %d Was %d", expected, actual);
                break;
        }
    }
}

/* Assert Equal String */
void UnityAssertEqualString(const char* expected, const char* actual, 
                           const char* msg, const uint32_t lineNumber)
{
    if ((expected == NULL) && (actual == NULL)) {
        return; /* Both NULL, considered equal */
    }
    
    if ((expected == NULL) || (actual == NULL)) {
        Unity.CurrentTestFailed = 1;
        Unity.CurrentTestLineNumber = lineNumber;
        printf("\n%s:%u:", Unity.TestFile, lineNumber);
        if (msg != NULL) {
            printf("%s ", msg);
        }
        printf("Expected '%s' Was '%s'", expected ? expected : "NULL", actual ? actual : "NULL");
        return;
    }
    
    if (strcmp(expected, actual) != 0) {
        Unity.CurrentTestFailed = 1;
        Unity.CurrentTestLineNumber = lineNumber;
        printf("\n%s:%u:", Unity.TestFile, lineNumber);
        if (msg != NULL) {
            printf("%s ", msg);
        }
        printf("Expected '%s' Was '%s'", expected, actual);
    }
}

/* Assert Bits */
void UnityAssertBits(const int32_t mask, const int32_t expected, const int32_t actual, 
                    const char* msg, const uint32_t lineNumber)
{
    if ((mask & expected) != (mask & actual)) {
        Unity.CurrentTestFailed = 1;
        Unity.CurrentTestLineNumber = lineNumber;
        printf("\n%s:%u:", Unity.TestFile, lineNumber);
        if (msg != NULL) {
            printf("%s ", msg);
        }
        printf("Expected 0x%08X Was 0x%08X", (mask & expected), (mask & actual));
    }
}

/* Assert Equal Memory */
void UnityAssertEqualMemory(const void* expected, const void* actual, 
                           const uint32_t length, const uint32_t num_elements,
                           const char* msg, const uint32_t lineNumber)
{
    if (memcmp(expected, actual, length * num_elements) != 0) {
        Unity.CurrentTestFailed = 1;
        Unity.CurrentTestLineNumber = lineNumber;
        printf("\n%s:%u:", Unity.TestFile, lineNumber);
        if (msg != NULL) {
            printf("%s ", msg);
        }
        printf("Memory Not Equal");
    }
}

/* Fail Test */
void UnityFail(const char* msg, const uint32_t line)
{
    Unity.CurrentTestFailed = 1;
    Unity.CurrentTestLineNumber = line;
    printf("\n%s:%u:%s", Unity.TestFile, line, msg);
}

/* Ignore Test */
void UnityIgnore(const char* msg, const uint32_t line)
{
    Unity.CurrentTestIgnored = 1;
    Unity.CurrentTestLineNumber = line;
    if (msg != NULL) {
        printf("\n%s:%u:%s", Unity.TestFile, line, msg);
    }
}
