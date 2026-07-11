#ifndef SGL_TYPE_H_
#define SGL_TYPE_H_

#if defined(_MSC_VER)

typedef signed char         sgl_int8_t;
typedef unsigned char       sgl_uint8_t;
typedef signed short        sgl_int16_t;
typedef unsigned short      sgl_uint16_t;
typedef signed int          sgl_int32_t;
typedef unsigned int        sgl_uint32_t;
typedef signed __int64      sgl_int64_t;
typedef unsigned __int64    sgl_uint64_t;

#if defined(_WIN64)
typedef signed __int64      sgl_intptr_t;
typedef unsigned __int64    sgl_uintptr_t;
typedef unsigned __int64    sgl_size_t;
typedef signed __int64      sgl_ptrdiff_t;
#else
typedef signed int          sgl_intptr_t;
typedef unsigned int        sgl_uintptr_t;
typedef unsigned int        sgl_size_t;
typedef signed int          sgl_ptrdiff_t;
#endif

#elif defined(__INT8_TYPE__) && defined(__UINT8_TYPE__) && \
      defined(__INT16_TYPE__) && defined(__UINT16_TYPE__) && \
      defined(__INT32_TYPE__) && defined(__UINT32_TYPE__) && \
      defined(__INT64_TYPE__) && defined(__UINT64_TYPE__) && \
      defined(__INTPTR_TYPE__) && defined(__UINTPTR_TYPE__) && \
      defined(__SIZE_TYPE__) && defined(__PTRDIFF_TYPE__)

typedef __INT8_TYPE__       sgl_int8_t;
typedef __UINT8_TYPE__      sgl_uint8_t;
typedef __INT16_TYPE__      sgl_int16_t;
typedef __UINT16_TYPE__     sgl_uint16_t;
typedef __INT32_TYPE__      sgl_int32_t;
typedef __UINT32_TYPE__     sgl_uint32_t;
typedef __INT64_TYPE__      sgl_int64_t;
typedef __UINT64_TYPE__     sgl_uint64_t;
typedef __INTPTR_TYPE__     sgl_intptr_t;
typedef __UINTPTR_TYPE__    sgl_uintptr_t;
typedef __SIZE_TYPE__       sgl_size_t;
typedef __PTRDIFF_TYPE__    sgl_ptrdiff_t;

#elif defined(__CPPCHECK__)

typedef signed char         sgl_int8_t;
typedef unsigned char       sgl_uint8_t;
typedef signed short        sgl_int16_t;
typedef unsigned short      sgl_uint16_t;
typedef signed int          sgl_int32_t;
typedef unsigned int        sgl_uint32_t;
typedef signed long long    sgl_int64_t;
typedef unsigned long long  sgl_uint64_t;
typedef signed long         sgl_intptr_t;
typedef unsigned long       sgl_uintptr_t;
typedef unsigned long       sgl_size_t;
typedef signed long         sgl_ptrdiff_t;

#else
/* cppcheck-suppress preprocessorErrorDirective */
#error "Unsupported compiler: fixed-width predefined types are unavailable"
#endif

#if defined(__cplusplus)
typedef bool                sgl_bool_t;
#elif defined(_MSC_VER) && !defined(__clang__)
typedef unsigned char       sgl_bool_t;
#else
typedef _Bool               sgl_bool_t;
#endif

#define SGL_FALSE           ((sgl_bool_t)0)
#define SGL_TRUE            ((sgl_bool_t)1)
#define SGL_SIZE_MAX        ((sgl_size_t)-1)

#if defined(__cplusplus)
#define SGL_NULL            0
#else
#define SGL_NULL            ((void *)0)
#endif

#endif  /* SGL_TYPE_H_ */
