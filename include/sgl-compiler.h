/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Dylan Hong
 *
 * This code is released under the MIT License.
 * For conditions of distribution and use, see the LICENSE file.
 */
#ifndef SGL_COMPILER_H_
#define SGL_COMPILER_H_

/*
 *******************************************************************
 *                          COMPILER
 *******************************************************************
 */
/*
 * Prefetch contract
 * -----------------
 * byte_distance is the look-ahead offset from address, not a byte count to
 * load.  The caller must keep address + byte_distance within the same allocated
 * object.  A distance of zero prefetches address itself.  Prefetch is an
 * optional performance hint and unsupported compilers keep this API as a no-op.
 * rw and locality must be compile-time constants for GCC and Clang.
 */
#define SGL_PREFETCH_RW_READ                  (0)
#define SGL_PREFETCH_RW_WRITE                 (1)

#define SGL_PREFETCH_LOCALITY_NON_TEMPORAL    (0)
#define SGL_PREFETCH_LOCALITY_LOW             (1)
#define SGL_PREFETCH_LOCALITY_MODERATE        (2)
#define SGL_PREFETCH_LOCALITY_HIGH            (3)

/*
 * Branch prediction contract
 * --------------------------
 * Use SGL_LIKELY and SGL_UNLIKELY only when one outcome is consistently
 * dominant across supported workloads.  Input-dependent hints can penalize
 * the opposite workload, and unnecessary hints can inhibit branchless code
 * generation.
 */
#if defined(_MSC_VER)

#define SGL_ALWAYS_INLINE             __forceinline
#define SGL_NOINLINE                  __declspec(noinline)
#define SGL_RESTRICT                  __restrict
#define SGL_ALIGNED(n)                __declspec(align(n))
#define SGL_PACKED
#define SGL_NORETURN                  __declspec(noreturn)
#define SGL_DEPRECATED(message)       __declspec(deprecated(message))
#define SGL_WARN_UNUSED_RESULT
#define SGL_LIKELY(expression)        (!!(expression))
#define SGL_UNLIKELY(expression)      (!!(expression))
#define SGL_FALLTHROUGH
#define SGL_UNREACHABLE()             __assume(0)
#define SGL_CPU_RELAX()               ((void)0)
#define SGL_PREFETCH(address, byte_distance, rw, locality) \
    ((void)(address), (void)(byte_distance), (void)(rw), (void)(locality))

#elif defined(__GNUC__) || defined(__clang__)

#define SGL_ALWAYS_INLINE             inline __attribute__((always_inline))
#define SGL_NOINLINE                  __attribute__((noinline))
#define SGL_RESTRICT                  __restrict__
#define SGL_ALIGNED(n)                __attribute__((aligned(n)))
#define SGL_PACKED                    __attribute__((packed))
#define SGL_NORETURN                  __attribute__((noreturn))
#define SGL_DEPRECATED(message)       __attribute__((deprecated(message)))
#define SGL_WARN_UNUSED_RESULT        __attribute__((warn_unused_result))
#define SGL_LIKELY(expression)        __builtin_expect(!!(expression), 1)
#define SGL_UNLIKELY(expression)      __builtin_expect(!!(expression), 0)
#define SGL_FALLTHROUGH               __attribute__((fallthrough))
#define SGL_UNREACHABLE()             __builtin_unreachable()
#if defined(__aarch64__) || defined(__arm__)
#define SGL_CPU_RELAX()               __asm__ __volatile__("yield")
#elif defined(__i386__) || defined(__x86_64__)
#define SGL_CPU_RELAX()               __asm__ __volatile__("pause")
#else
#define SGL_CPU_RELAX()               __asm__ __volatile__("" ::: "memory")
#endif
#define SGL_PREFETCH(address, byte_distance, rw, locality) \
    __builtin_prefetch( \
        ((const char *)(address)) + (byte_distance), \
        (rw), \
        (locality))

#else

#define SGL_ALWAYS_INLINE             inline
#define SGL_NOINLINE
#define SGL_RESTRICT
#define SGL_ALIGNED(n)
#define SGL_PACKED
#define SGL_NORETURN
#define SGL_DEPRECATED(message)
#define SGL_WARN_UNUSED_RESULT
#define SGL_LIKELY(expression)        (!!(expression))
#define SGL_UNLIKELY(expression)      (!!(expression))
#define SGL_FALLTHROUGH
#define SGL_UNREACHABLE()             ((void)0)
#define SGL_CPU_RELAX()               ((void)0)
#define SGL_PREFETCH(address, byte_distance, rw, locality) \
    ((void)(address), (void)(byte_distance), (void)(rw), (void)(locality))

#endif

#define SGL_PREFETCH_READ(address, byte_distance) \
    SGL_PREFETCH( \
        address, \
        byte_distance, \
        SGL_PREFETCH_RW_READ, \
        SGL_PREFETCH_LOCALITY_HIGH)

#define SGL_PREFETCH_WRITE(address, byte_distance) \
    SGL_PREFETCH( \
        address, \
        byte_distance, \
        SGL_PREFETCH_RW_WRITE, \
        SGL_PREFETCH_LOCALITY_HIGH)

#define SGL_UNUSED(value)             ((void)(value))

#endif  /* SGL_COMPILER_H_ */
