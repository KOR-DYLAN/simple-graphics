#ifndef SGL_FIXED_POINT__H__
#define SGL_FIXED_POINT__H__

#include "fixed_point/generic.h"

/************************************************************
 * SIMD Supported
 ************************************************************/
#if SGL_CFG_HAS_NEON
#include "fixed_point/neon.h"
#endif  /* !SGL_CFG_HAS_NEON */

#endif  /* !SGL_FIXED_POINT__H__ */
