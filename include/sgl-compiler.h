#ifndef SGL_COMPILER_H_
#define SGL_COMPILER_H_

/*
 *******************************************************************
 *                          COMPILER
 *******************************************************************
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

#endif

#define SGL_UNUSED(value)             ((void)(value))

#endif  /* SGL_COMPILER_H_ */
