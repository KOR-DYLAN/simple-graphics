#ifndef SGL_FIXED_POINT__H__
#define SGL_FIXED_POINT__H__

#include <sgl-compiler.h>
#include <sgl-type.h>
#include <sgl-config.h>

#include "fixed_point/generic.h"

/************************************************************
 * SIMD Supported
 ************************************************************/
#if defined(SGL_CFG_HAS_NEON)
#include "fixed_point/neon.h"
#endif  /* !SGL_CFG_HAS_NEON */

#endif  /* !SGL_FIXED_POINT__H__ */
