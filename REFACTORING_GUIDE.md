# Code Refactoring and Testing Guide

**Target:** Onion OS for Miyoo Mini+  
**Focus:** Clean code, testability, maintainability  
**Last Updated:** 2026-02-02

---

## Table of Contents

1. [Introduction](#introduction)
2. [SOLID Principles Applied](#solid-principles-applied)
3. [Header Guards Standard](#header-guards-standard)
4. [C11 Standard Features](#c11-standard-features)
5. [Unit Testing with Unity](#unit-testing-with-unity)
6. [Refactoring Examples](#refactoring-examples)
7. [CI/CD Integration](#cicd-integration)
8. [Best Practices](#best-practices)

---

## Introduction

This guide documents the refactoring and testing improvements made to the Onion OS codebase. The goal is to improve:

- **Code Quality** - Cleaner, more maintainable code
- **Testability** - Unit tests for critical functionality
- **Modularity** - Better separation of concerns
- **Safety** - Type safety and compile-time checks
- **Standards Compliance** - C11 standard adoption

---

## SOLID Principles Applied

### Single Responsibility Principle (SRP)

**Before:** Monolithic functions handling multiple concerns
```c
// BAD: Function does too much
void processGame(Game *game) {
    // Load ROM
    // Parse metadata
    // Update database
    // Render UI
    // Handle input
}
```

**After:** Separate functions with single responsibilities
```c
// GOOD: Each function has one responsibility
void loadROM(const char *path, ROM *rom);
void parseMetadata(ROM *rom, Metadata *meta);
void updateDatabase(const Metadata *meta);
void renderGameUI(const Game *game);
void handleGameInput(const Input *input);
```

### Open/Closed Principle (OCP)

**Strategy:** Use function pointers and interfaces for extensibility

```c
// Interface for display operations
typedef struct {
    void (*init)(void);
    void (*draw)(const uint32_t *buffer);
    void (*clear)(void);
} DisplayDriver;

// Different implementations without modifying core code
DisplayDriver miyoo_mini_driver = {
    .init = miyoo_mini_init,
    .draw = miyoo_mini_draw,
    .clear = miyoo_mini_clear
};

DisplayDriver miyoo_mini_plus_driver = {
    .init = miyoo_mini_plus_init,
    .draw = miyoo_mini_plus_draw,
    .clear = miyoo_mini_plus_clear
};
```

### Liskov Substitution Principle (LSP)

**Applied in:** Device model abstraction

```c
// Base device interface
typedef struct Device {
    int (*getBatteryLevel)(void);
    void (*setVolume)(int level);
} Device;

// Implementations are substitutable
Device miyoo283;
Device miyoo354;
```

### Interface Segregation Principle (ISP)

**Strategy:** Small, focused interfaces instead of large ones

```c
// BAD: Fat interface
typedef struct {
    void (*read)(void);
    void (*write)(void);
    void (*seek)(void);
    void (*network_send)(void);  // Not all files need this!
} FileOperations;

// GOOD: Segregated interfaces
typedef struct {
    void (*read)(void);
    void (*write)(void);
} BasicFileOps;

typedef struct {
    void (*send)(void);
    void (*receive)(void);
} NetworkOps;
```

### Dependency Inversion Principle (DIP)

**Applied in:** Dependency injection for testability

```c
// High-level module depends on abstraction
typedef struct {
    int (*send)(const char *cmd);
    int (*receive)(char *buffer, size_t size);
} CommandInterface;

// Implementation can be real or mock
void processCommand(CommandInterface *iface, const char *cmd) {
    iface->send(cmd);
    // ...
}
```

---

## Header Guards Standard

### Convention

All header files use the format: `<MODULE>_<FILE>_H__`

**Examples:**
```c
// src/common/utils/str.h
#ifndef UTILS_STR_H__
#define UTILS_STR_H__
// ...
#endif // UTILS_STR_H__

// src/common/system/display.h
#ifndef SYSTEM_DISPLAY_H__
#define SYSTEM_DISPLAY_H__
// ...
#endif // SYSTEM_DISPLAY_H__

// src/gameSwitcher/gs_overlay.h
#ifndef GAMESWITCHER_GS_OVERLAY_H__
#define GAMESWITCHER_GS_OVERLAY_H__
// ...
#endif // GAMESWITCHER_GS_OVERLAY_H__
```

### Benefits

1. **Namespace collision prevention** - Unique guard names
2. **Consistency** - Easy to identify file from guard
3. **Tooling support** - IDE autocomplete works better
4. **Maintainability** - Clear naming convention

### Migration Script

```bash
#!/bin/bash
# Update header guards to new standard

for file in $(find src/ -name "*.h"); do
    # Extract module and filename
    module=$(dirname "$file" | sed 's/.*\///' | tr 'a-z' 'A-Z')
    filename=$(basename "$file" .h | tr 'a-z' 'A-Z')
    
    # Generate new guard
    guard="${module}_${filename}_H__"
    
    # Update file (implementation left as exercise)
    echo "Would update $file with guard $guard"
done
```

---

## C11 Standard Features

### Static Assertions

**Use for compile-time validation:**

```c
#include <assert.h>

// Ensure structure sizes are as expected
_Static_assert(sizeof(int) == 4, "int must be 4 bytes");
_Static_assert(sizeof(Game_s) <= 1024, "Game structure too large");

// Ensure proper alignment
_Static_assert(_Alignof(DisplayBuffer) == 64, "Display buffer must be cache-aligned");
```

### Anonymous Structs and Unions

**Simplify nested structures:**

```c
// C11 allows anonymous structs
typedef struct {
    union {
        struct {
            uint8_t r, g, b, a;
        };
        uint32_t rgba;
    };
} Color;

// Usage is cleaner
Color c;
c.r = 255;
c.g = 128;
c.b = 0;
uint32_t packed = c.rgba;  // Access packed form
```

### Alignment Specifiers

**Optimize for cache and SIMD:**

```c
// Align to cache line for better performance
_Alignas(64) uint32_t framebuffer[640 * 480];

// Align for NEON operations
_Alignas(16) float vector_data[4];
```

### Thread-Local Storage

**Safe per-thread state:**

```c
#include <threads.h>

// Thread-local error code
_Thread_local int last_error = 0;

void setError(int code) {
    last_error = code;  // Each thread has its own copy
}
```

### Atomic Operations

**Lock-free programming:**

```c
#include <stdatomic.h>

atomic_int save_state_status = ATOMIC_VAR_INIT(0);

void updateStatus(int new_status) {
    atomic_store(&save_state_status, new_status);
}

int getStatus(void) {
    return atomic_load(&save_state_status);
}
```

---

## Unit Testing with Unity

### Why Unity?

1. **Lightweight** - Perfect for embedded systems
2. **C-only** - No C++ dependencies
3. **Simple** - Easy to learn and use
4. **Portable** - Works on any platform

### Basic Test Structure

```c
#include "unity.h"

/* Setup before each test */
void setUp(void) {
    // Initialize test fixtures
}

/* Teardown after each test */
void tearDown(void) {
    // Clean up
}

/* Individual test */
void test_addition(void) {
    TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

/* Main test runner */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_addition);
    return UNITY_END();
}
```

### Assertion Types

```c
// Boolean assertions
TEST_ASSERT_TRUE(condition);
TEST_ASSERT_FALSE(condition);

// Equality assertions
TEST_ASSERT_EQUAL_INT(expected, actual);
TEST_ASSERT_EQUAL_UINT(expected, actual);
TEST_ASSERT_EQUAL_HEX32(expected, actual);
TEST_ASSERT_EQUAL_STRING(expected, actual);

// Pointer assertions
TEST_ASSERT_NULL(pointer);
TEST_ASSERT_NOT_NULL(pointer);

// Memory comparison
TEST_ASSERT_EQUAL_MEMORY(expected, actual, length);
```

### Mocking Dependencies

**Example: Mock UDP for RetroArch tests**

```c
/* Mock implementation */
static char mock_response[1024];
static int mock_result = 0;

int udp_send(const char *ip, int port, const char *msg) {
    (void)ip; (void)port; (void)msg;
    return mock_result;
}

/* Test with mock */
void test_retroarch_command(void) {
    mock_result = 0;  // Success
    int result = retroarch_quit();
    TEST_ASSERT_EQUAL_INT(0, result);
}
```

---

## Refactoring Examples

### Example 1: String Utilities

**Before:** Unsafe, no error checking
```c
void concatenate(char *dest, const char *src) {
    strcat(dest, src);  // Buffer overflow risk!
}
```

**After:** Safe with bounds checking
```c
bool concatenate(char *dest, size_t dest_size, const char *src) {
    if (!dest || !src || dest_size == 0) {
        return false;
    }
    
    size_t dest_len = strnlen(dest, dest_size);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) {
        return false;  // Would overflow
    }
    
    strncat(dest, src, dest_size - dest_len - 1);
    return true;
}
```

### Example 2: File Operations

**Before:** No error handling
```c
void readConfig(const char *path) {
    FILE *fp = fopen(path, "r");
    char buffer[256];
    fgets(buffer, sizeof(buffer), fp);
    fclose(fp);
}
```

**After:** Proper error handling
```c
typedef enum {
    CONFIG_OK,
    CONFIG_FILE_NOT_FOUND,
    CONFIG_READ_ERROR,
    CONFIG_PARSE_ERROR
} ConfigError;

ConfigError readConfig(const char *path, Config *config) {
    if (!path || !config) {
        return CONFIG_PARSE_ERROR;
    }
    
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return CONFIG_FILE_NOT_FOUND;
    }
    
    char buffer[256];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fclose(fp);
        return CONFIG_READ_ERROR;
    }
    
    fclose(fp);
    
    // Parse config...
    return CONFIG_OK;
}
```

### Example 3: RetroArch Command Refactoring

**Before:** Monolithic function
```c
int handleRetroArchCommand(const char *cmd) {
    // Parse command
    // Send to RetroArch
    // Wait for response
    // Parse response
    // Update state
    // Return result
}
```

**After:** Modular with testable parts
```c
typedef struct {
    CommandType type;
    char params[256];
} ParsedCommand;

ParsedCommand parseCommand(const char *cmd);
int sendCommand(const ParsedCommand *cmd);
Response receiveResponse(void);
int parseResponse(const Response *resp);
void updateState(const Response *resp);

// Main function orchestrates
int handleRetroArchCommand(const char *cmd) {
    ParsedCommand parsed = parseCommand(cmd);
    if (parsed.type == CMD_INVALID) {
        return -1;
    }
    
    if (sendCommand(&parsed) != 0) {
        return -1;
    }
    
    Response resp = receiveResponse();
    int result = parseResponse(&resp);
    
    if (result == 0) {
        updateState(&resp);
    }
    
    return result;
}
```

---

## CI/CD Integration

### GitHub Actions Workflow

The project includes a comprehensive CI/CD pipeline:

**`.github/workflows/ci.yml`** includes:

1. **Unit Tests** - Run all unit tests
2. **Coverage** - Generate code coverage reports
3. **Static Analysis** - Check code quality
4. **Build Verification** - Ensure project builds
5. **Documentation** - Verify docs are present

### Running Tests Locally

```bash
# Build and run all tests
cd test
make -f Makefile.unity test

# Generate coverage report
make -f Makefile.unity coverage

# Clean and rebuild
make -f Makefile.unity clean all
```

### Test Organization

```
test/
├── unity/              # Unity framework
│   ├── unity.h
│   └── unity.c
├── unit/               # Unit tests
│   ├── test_str_utils.c
│   ├── test_file_utils.c
│   └── test_retroarch_cmd.c
├── integration/        # Integration tests
├── mocks/              # Mock implementations
└── Makefile.unity      # Build system
```

---

## Best Practices

### Code Organization

1. **One module, one responsibility**
2. **Header files declare interface only**
3. **Implementation details in .c files**
4. **Use static for internal functions**

### Error Handling

```c
// Use enum for error codes
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_PARAM = -1,
    ERR_OUT_OF_MEMORY = -2,
    ERR_IO_FAILURE = -3
} ErrorCode;

// Return error codes, use output parameters for results
ErrorCode loadFile(const char *path, char **contents);
```

### Memory Management

```c
// Always check malloc
void *ptr = malloc(size);
if (!ptr) {
    return ERR_OUT_OF_MEMORY;
}

// Free in reverse order of allocation
// Use goto for cleanup (single exit point)
ErrorCode complexFunction(void) {
    char *buffer = NULL;
    FILE *fp = NULL;
    ErrorCode err = ERR_OK;
    
    buffer = malloc(SIZE);
    if (!buffer) {
        err = ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    fp = fopen("file", "r");
    if (!fp) {
        err = ERR_IO_FAILURE;
        goto cleanup;
    }
    
    // Do work...
    
cleanup:
    if (fp) fclose(fp);
    free(buffer);
    return err;
}
```

### Documentation

```c
/**
 * @brief Brief description of function
 * 
 * Detailed description of what the function does,
 * any side effects, and important notes.
 * 
 * @param[in]  input   Description of input parameter
 * @param[out] output  Description of output parameter
 * @return Error code (0 on success, negative on error)
 * 
 * @note Important notes about usage
 * @warning Warnings about potential issues
 */
int myFunction(const char *input, char **output);
```

---

## Summary

### Improvements Made

1. ✅ **Test Infrastructure** - Unity framework integrated
2. ✅ **Unit Tests** - Tests for string, file, and RetroArch utilities
3. ✅ **CI/CD** - GitHub Actions workflow
4. ✅ **Standards** - Header guard convention
5. ✅ **C11 Features** - Modern C standard adoption
6. ✅ **SOLID Principles** - Applied throughout refactoring
7. ✅ **Documentation** - Comprehensive guides

### Next Steps

1. Add more unit tests for remaining modules
2. Implement integration tests
3. Set up automated code coverage tracking
4. Migrate more code to C11 features
5. Add performance benchmarks
6. Create developer onboarding guide

---

**Author:** GitHub Copilot Coding Agent  
**Version:** 1.0  
**Last Updated:** 2026-02-02
