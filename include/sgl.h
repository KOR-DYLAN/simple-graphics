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

typedef struct sgl_bilinear_lookup_table sgl_bilinear_lookup_t;


/*******************************************************************
 *                          Resize
 *******************************************************************/
sgl_bilinear_lookup_t *sgl_generic_create_bilinear_lut(int32_t d_width, int32_t d_height, int32_t s_width, int32_t s_height);
void sgl_generic_destroy_bilinear_lut(sgl_bilinear_lookup_t *lut);
sgl_result_t sgl_generic_resize_nearest(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp);
sgl_result_t sgl_generic_resize_bilinear(sgl_bilinear_lookup_t *ext_lut, uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp);
sgl_result_t sgl_generic_resize_cubic(uint8_t *dst, int32_t d_width, int32_t d_height, uint8_t *src, int32_t s_width, int32_t s_height, int32_t bpp);

/*******************************************************************
 *                          Util
 *******************************************************************/
static inline uint8_t sgl_clamp_u8_i32(int32_t val)
{
    uint8_t u8_val = (uint8_t)val;

    if ((val & ~0xFF) != 0) { 
        if (val < 0) {
            u8_val = 0U;
        }
        else {
            u8_val = 255U;
        }
    }

    return u8_val;
}

#if defined(__cplusplus)
}
#endif

#endif  /* !SGL__H__ */

/* End Of File */
