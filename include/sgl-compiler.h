#ifndef SGL_COMPILER_H_
#define SGL_COMPILER_H_

#if defined(_MSC_VER)
    #define SGL_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define SGL_ALWAYS_INLINE inline __attribute__((always_inline))
#else
    #define SGL_ALWAYS_INLINE inline
#endif

#if defined(_MSC_VER)
    #define SGL_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
    #define SGL_RESTRICT __restrict__
#else
    #define SGL_RESTRICT
#endif

#if defined(_MSC_VER)
    #define SGL_ALIGNED(n) __declspec(align(n))
#elif defined(__GNUC__) || defined(__clang__)
    #define SGL_ALIGNED(n) __attribute__((aligned(n)))
#else
    #define SGL_ALIGNED(n)
#endif

#endif  /* !SGL_COMPILER_H_ */
