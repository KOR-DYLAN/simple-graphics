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

/*
 *******************************************************************
 *                          DEFINES
 *******************************************************************
 */
#define SGL_DIV_ROUNDUP(n, d)   (((n) + (d) - 1) / (d))
#define SGL_SAFE_FREE(p)        if ((p) != NULL) { free(p); (p) = NULL; }


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
} sgl_result_t;


/*******************************************************************
 *                          Resize
 *******************************************************************/
sgl_result_t sgl_generic_resize_nearest(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp);
sgl_result_t sgl_generic_resize_bilinear(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp);
sgl_result_t sgl_generic_resize_cubic(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp);



#if defined(__cplusplus)
}
#endif

#endif  /* !SGL__H__ */

/* End Of File */
