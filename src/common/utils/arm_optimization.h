#ifndef UTILS_ARM_OPTIMIZATION_H__
#define UTILS_ARM_OPTIMIZATION_H__

/**
 * ARM Cortex-A7 optimization utilities
 * 
 * Provides branch prediction hints and alignment hints for single-core ARMv7
 * architectures with limited cache and high context-switch costs.
 */

#ifdef __GNUC__
    // Branch prediction hints for Cortex-A7 pipeline
    // Use for hot paths to reduce branch misprediction penalties
    #define likely(x)       __builtin_expect(!!(x), 1)
    #define unlikely(x)     __builtin_expect(!!(x), 0)
    
    // Prefetch hint for memory-bound operations
    #define prefetch(addr)  __builtin_prefetch(addr)
    
    // Force inline for small hot functions (< 10 instructions)
    #define FORCE_INLINE    __attribute__((always_inline)) inline
    
    // Align to cache line (16 bytes on Cortex-A7 L1)
    #define CACHE_ALIGNED   __attribute__((aligned(16)))
#else
    #define likely(x)       (x)
    #define unlikely(x)     (x)
    #define prefetch(addr)  ((void)0)
    #define FORCE_INLINE    inline
    #define CACHE_ALIGNED
#endif

/**
 * restrict pointer hint for auto-vectorization
 * Informs compiler that pointers do not alias, enabling NEON optimizations
 */
#ifndef restrict
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define restrict __restrict__
    #else
        #define restrict
    #endif
#endif

#endif // UTILS_ARM_OPTIMIZATION_H__
