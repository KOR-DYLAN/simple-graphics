#ifndef SGL__H__
#define SGL__H__

#if defined(__cplusplus)
extern "C" {
#endif
/*
 *******************************************************************
 *                          INCLUDES
 *******************************************************************
 */
#if !defined(__cplusplus)
#   include <stdbool.h>
#endif
#include <stddef.h>
#include <stdint.h>
#include <sgl-config.h>

/*
 *******************************************************************
 *                          COMPILER
 *******************************************************************
 */
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


/*
 *******************************************************************
 *                          DEFINES
 *******************************************************************
 */
#define SGL_UNUSED_PARAM(p)                         (void)(p)
#define SGL_DIV_ROUNDUP(n, d)                       (((n) + (d) - 1) / (d))
#define SGL_SAFE_FREE(p)                            if ((p) != NULL) { free(p); (p) = NULL; }
#define SGL_THREADPOOL_DEFAULT_MAX_ROUTINE_LISTS    (4U)
#define SGL_GENERIC_BULK_SIZE                       (4)
#define SGL_SIMD_BULK_SIZE                          (8)
#define SGL_BPP32                                   (4)
#define SGL_BPP24                                   (3)
#define SGL_BPP16                                   (2)
#define SGL_BPP8                                    (1)


/*
 *******************************************************************
 *                          TYPEDEF
 *******************************************************************
 */
typedef enum {
    SGL_SUCCESS,
    SGL_FAILURE,
    SGL_ERROR_INVALID_ARGUMENTS,
    SGL_ERROR_MEMORY_ALLOCATION,
    SGL_ERROR_MISSMATCHED_CAPACITY,
    SGL_ERROR_NOT_SUPPORTED,
    SGL_QUEUE_IS_EMPTY,
    SGL_QUEUE_IS_NOT_EMPTY,
    SGL_QUEUE_IS_FULL,
    SGL_QUEUE_IS_NOT_FULL,
} sgl_result_t;

typedef struct sgl_nearest_neighbor_lookup_table    sgl_nearest_neighbor_lookup_t;
typedef struct sgl_bilinear_lookup_table            sgl_bilinear_lookup_t;
typedef struct sgl_queue                            sgl_queue_t;
typedef struct sgl_threadpool                       sgl_threadpool_t;
typedef void(*sgl_threadpool_routine_t)(void *SGL_RESTRICT current, void *SGL_RESTRICT cookie);


/*******************************************************************
 *                          Resize
 *******************************************************************/
sgl_nearest_neighbor_lookup_t *sgl_generic_create_nearest_neighbor_lut(int32_t d_width, int32_t d_height, int32_t s_width, int32_t s_height);
sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(int32_t d_width, int32_t d_height, int32_t s_width, int32_t s_height);

void sgl_generic_destroy_nearest_neighbor_lut(sgl_nearest_neighbor_lookup_t *lut);
void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut);

/* Generic Resize */
sgl_result_t sgl_generic_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp);

sgl_result_t sgl_generic_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp);

sgl_result_t sgl_generic_resize_cubic(uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, int32_t bpp);

/* SIMD Resize */
#if SGL_CFG_HAS_SIMD
sgl_result_t sgl_simd_resize_nearest(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_nearest_neighbor_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp);

sgl_result_t sgl_simd_resize_bilinear(
                sgl_threadpool_t *SGL_RESTRICT pool, sgl_bilinear_lookup_t *SGL_RESTRICT ext_lut, 
                uint8_t *SGL_RESTRICT dst, int32_t d_width, int32_t d_height, 
                uint8_t *SGL_RESTRICT src, int32_t s_width, int32_t s_height, 
                int32_t bpp);
#endif

/*******************************************************************
 *                          Queue
 *******************************************************************/
sgl_queue_t *sgl_queue_create(size_t capacity);
void sgl_queue_destroy(sgl_queue_t **queue);
sgl_result_t sgl_queue_copy(sgl_queue_t *SGL_RESTRICT dst, sgl_queue_t *SGL_RESTRICT src);
sgl_result_t sgl_queue_unsafe_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data);
sgl_result_t sgl_queue_enqueue(sgl_queue_t *SGL_RESTRICT queue, const void *SGL_RESTRICT data);
const void *sgl_queue_dequeue(sgl_queue_t *queue);
const void *sgl_queue_peek(sgl_queue_t *queue);
sgl_result_t sgl_queue_is_empty(sgl_queue_t *queue);
sgl_result_t sgl_queue_is_full(sgl_queue_t *queue);
size_t sgl_queue_get_capacity(sgl_queue_t *queue);
size_t sgl_queue_get_count(sgl_queue_t *queue);


/*******************************************************************
 *                          Threadpool
 *******************************************************************/
#if SGL_CFG_HAS_THREAD
sgl_threadpool_t *sgl_threadpool_create(size_t num_threads, size_t max_routine_lists, const char *base_name);
sgl_result_t sgl_threadpool_destroy(sgl_threadpool_t *pool);
sgl_result_t sgl_threadpool_attach_routine(sgl_threadpool_t *SGL_RESTRICT pool, sgl_threadpool_routine_t routine, sgl_queue_t *SGL_RESTRICT operations, void *SGL_RESTRICT cookie);
#endif  /* !SGL_CFG_HAS_THREAD */

#if defined(__cplusplus)
}
#endif

#endif  /* !SGL__H__ */

/* End Of File */
