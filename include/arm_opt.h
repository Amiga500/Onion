#ifndef ARM_OPT_H
#define ARM_OPT_H

/**
 * ARM Cortex-A7 Low-Level Optimization Hints
 * 
 * This header provides inline hints and macros specifically tuned for
 * ARM Cortex-A7 processors (Miyoo Mini+) to maximize performance.
 * 
 * Key optimizations:
 * - Force inlining of hot-path functions (eliminates call overhead ~4-8 cycles)
 * - Branch prediction hints (improves pipeline efficiency)
 * - Memory prefetch hints (reduces cache miss latency)
 * - NEON SIMD availability detection
 */

// Force aggressive inlining for critical functions
// Saves ~4-8 CPU cycles per call by eliminating function call overhead
#ifdef __GNUC__
    #define ALWAYS_INLINE __attribute__((always_inline)) inline
    #define NEVER_INLINE __attribute__((noinline))
    #define HOT_FUNCTION __attribute__((hot))
    #define COLD_FUNCTION __attribute__((cold))
#else
    #define ALWAYS_INLINE inline
    #define NEVER_INLINE
    #define HOT_FUNCTION
    #define COLD_FUNCTION
#endif

// Branch prediction hints - helps ARM pipeline stay full
// Correct prediction saves ~10-20 cycles on branch misprediction
#ifdef __GNUC__
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// Memory alignment hint for ARM NEON (16-byte alignment)
// Unaligned access can be 2-3x slower on ARM
#ifdef __GNUC__
    #define ALIGN_16 __attribute__((aligned(16)))
    #define ALIGN_32 __attribute__((aligned(32)))
#else
    #define ALIGN_16
    #define ALIGN_32
#endif

// Prefetch hint for next cache line (reduces cache miss penalty)
// ARM Cortex-A7 has L1 cache line size of 32 bytes
#ifdef __GNUC__
    #define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch(addr, 1, 3)
#else
    #define PREFETCH(addr)
    #define PREFETCH_WRITE(addr)
#endif

// Check if NEON SIMD is available at compile time
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
    #define HAS_NEON 1
    #include <arm_neon.h>
#else
    #define HAS_NEON 0
#endif

// Fast bit manipulation macros (compile to single ARM instructions)
#define FAST_ABS(x) __builtin_abs(x)
#define FAST_CLZ(x) __builtin_clz(x)  // Count leading zeros - single CLZ instruction

// Memory barrier for cache coherency (important for DMA/hardware blitting)
#ifdef __GNUC__
    #define MEMORY_BARRIER() __sync_synchronize()
#else
    #define MEMORY_BARRIER()
#endif

// Restrict pointer keyword - tells compiler pointers don't alias
// Enables more aggressive optimization by compiler
#ifdef __GNUC__
    #define RESTRICT __restrict__
#else
    #define RESTRICT
#endif

#endif // ARM_OPT_H
