#ifndef SGL_COMPILER_H_
#define SGL_COMPILER_H_

#if defined(_MSC_VER)
    #define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define ALWAYS_INLINE inline __attribute__((always_inline))
#else
    #define ALWAYS_INLINE inline
#endif

#endif  /* !SGL_COMPILER_H_ */
