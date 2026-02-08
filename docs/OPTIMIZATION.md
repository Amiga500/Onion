# Onion Codebase Optimization

This document describes the optimization work performed on the Onion codebase, including code deduplication and assembly optimizations.

## Code Refactoring: Signal Handler Deduplication

### Problem
Multiple source files contained identical signal handler implementations for handling SIGINT and SIGTERM signals. This duplicated code appeared in at least 9 different files:

- `src/prompt/prompt.c`
- `src/chargingState/chargingState.c`
- `src/infoPanel/appstate.h`
- `src/tweaks/appstate.h`
- `src/gameSwitcher/gs_appState.h`
- `src/batteryMonitorUI/batteryMonitorUI.c`
- `src/playActivityUI/playActivityUI.c`
- `src/batmon/batmon.c`
- `src/keymon/keymon.c`

Each file had nearly identical code:
```c
static void sigHandler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        quit = true;
        break;
    default:
        break;
    }
}
```

### Solution
Created a new utility header `src/common/utils/signal_handler.h` that provides a reusable inline function:

```c
static inline void signal_handler_quit(volatile bool *quit_flag, int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        *quit_flag = true;
        break;
    default:
        break;
    }
}
```

### Usage
Files now use the common handler:

```c
#include "utils/signal_handler.h"

static bool quit = false;

static void sigHandler(int sig)
{
    signal_handler_quit(&quit, sig);
}

int main() {
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
    // ...
}
```

### Benefits
- **Reduced code duplication**: ~60 lines of duplicated code eliminated
- **Improved maintainability**: Signal handling logic is centralized
- **Consistent behavior**: All applications handle signals the same way
- **Easy to extend**: Future signal handling improvements only need to be made once

## Assembly Optimizations

### Existing NEON Optimizations

The Onion codebase already contains excellent ARM NEON SIMD assembly optimizations for performance-critical operations. These are located in `src/common/utils/neon_pixel.h`.

#### Optimized Operations

1. **Channel Swapping (ARGB ↔ RGBA)**
   - Function: `neon_swap_rb_inplace()`, `neon_argb_to_rgba()`
   - Uses: ARM NEON VLD4/VST4 instructions
   - Performance: Processes 16 pixels per iteration
   - Target: Cortex-A7 with NEON VFPv4

2. **Alpha Channel Processing**
   - Function: `neon_argb_to_rgba_alpha()`
   - Purpose: Alpha-conditional zeroing for PNG transparency
   - Uses: NEON vector compare and mask operations

3. **RGB888 to ARGB8888 Conversion**
   - Function: `neon_rgb888_to_argb()`
   - Purpose: 24-bit to 32-bit RGB expansion with alpha fill
   - Uses: NEON vector load/store with interleaving

4. **180° Rotation**
   - Function: `neon_rotate180_inplace()`
   - Performance: **50x faster than rotozoom** (noted in code comments)
   - Uses: VREV64 instruction for efficient pixel reversal
   - Processes: 8 pixels per iteration

#### Files Using NEON Optimizations

- `src/pngScale/pngScale.c` - Image scaling operations
- `src/common/utils/IMG_Save.h` - PNG export with per-row conversion
- Various graphics operations throughout the codebase

#### Code Example

From `pngScale.c`:
```c
// Delegates to shared NEON assembly in neon_pixel.h
static inline void swap_rb_channels(const uint32_t *src, uint32_t *dst, uint32_t count)
{
    if (src == dst) {
        /* In-place swap */
        neon_swap_rb_inplace(dst, (int)count);
    } else {
        neon_argb_to_rgba((uint32_t *)dst, src, (int)count);
    }
}
```

### Performance Notes

The inline assembly in `neon_pixel.h` includes:
- Compile-time target specification: `__attribute__((target("fpu=neon-vfpv4")))`
- Hand-optimized assembly blocks using ARM NEON intrinsics
- Fallback implementations for non-NEON builds
- Efficient memory access patterns

### Future Optimization Opportunities

While the codebase is already well-optimized, potential areas for further optimization include:

1. **String Operations**: The `str_replace()` function in `src/common/utils/str.c` uses malloc and string scanning in loops
2. **File Operations**: Some file copying operations could potentially benefit from larger buffers
3. **Database Queries**: SQLite operations in batmon and playActivity could use prepared statements for repeated queries

However, these are minor compared to the existing optimizations, and any changes should be validated with profiling data to ensure they provide measurable benefits.

## Conclusion

The Onion codebase demonstrates excellent optimization practices:
- **Code quality**: Good use of inline functions and headers for code reuse
- **Performance**: Strategic use of ARM NEON assembly for pixel operations
- **Architecture**: Well-organized source structure with common utilities

The refactoring work has further improved code maintainability by eliminating signal handler duplication, and the existing NEON optimizations provide significant performance benefits for graphics-intensive operations on the Miyoo Mini's ARM Cortex-A7 processor.
