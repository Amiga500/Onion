#ifndef ARM_HINTS_H__
#define ARM_HINTS_H__

/**
 * @file arm_hints.h
 * @brief ARM-specific optimization hints for Cortex-A7
 *
 * This header provides branch prediction hints and other optimization
 * primitives for the Miyoo Mini's ARM Cortex-A7 CPU. These hints help
 * the compiler generate better code by indicating expected branch outcomes.
 *
 * Hardware context:
 * - Single-core ARM Cortex-A7 @ up to 1.2GHz
 * - L1 cache: 32KB I-cache, 32KB D-cache
 * - L2 cache: 256KB (shared)
 * - Branch predictor: 2-level with 256-entry BTB
 *
 * Usage:
 *   if (likely(ptr != NULL)) { ... }    // Expected to be true
 *   if (unlikely(error_code)) { ... }   // Expected to be false
 */

/**
 * @brief Branch prediction hint for likely conditions
 *
 * Use when the condition is expected to be true in the common case.
 * Helps the compiler arrange basic blocks for better I-cache utilization
 * and reduces branch misprediction penalties (~13 cycles on Cortex-A7).
 *
 * @param x Condition expected to be true
 */
#define likely(x)   __builtin_expect(!!(x), 1)

/**
 * @brief Branch prediction hint for unlikely conditions
 *
 * Use when the condition is expected to be false in the common case.
 * Error paths, null checks that rarely fail, and exceptional conditions
 * are good candidates.
 *
 * @param x Condition expected to be false
 */
#define unlikely(x) __builtin_expect(!!(x), 0)

/**
 * @brief Cache line size for ARM Cortex-A7
 *
 * Use for aligning data structures to avoid false sharing and
 * maximize cache efficiency. L1 D-cache line is 64 bytes.
 */
#define CACHE_LINE_SIZE 64

/**
 * @brief Align structure/variable to cache line boundary
 *
 * Prevents false sharing when multiple threads access adjacent data,
 * and ensures single cache line loads for small hot structures.
 */
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

/**
 * @brief Prefetch data for read access
 *
 * Issues a PLD instruction to bring data into L1 D-cache.
 * Useful for prefetching next iteration's data in tight loops.
 *
 * @param addr Address to prefetch
 */
#define prefetch_read(addr)  __builtin_prefetch((addr), 0, 3)

/**
 * @brief Prefetch data for write access
 *
 * Issues a PLDW instruction to bring data into L1 D-cache with
 * exclusive access hint. Use when you know data will be written.
 *
 * @param addr Address to prefetch
 */
#define prefetch_write(addr) __builtin_prefetch((addr), 1, 3)

/**
 * @brief Maximum retry count for polling loops
 *
 * Used to bound spin-wait loops to prevent infinite hangs.
 * Set to allow ~500ms of retries with 10ms sleep intervals.
 */
#define POLL_MAX_RETRIES 50

/**
 * @brief Sleep interval for polling loops (microseconds)
 *
 * 10ms is a reasonable balance between latency and CPU/battery usage
 * for non-critical polling operations like input device grab.
 */
#define POLL_SLEEP_US 10000

#endif // ARM_HINTS_H__
