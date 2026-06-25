#ifndef SGL_OSAL_H_
#define SGL_OSAL_H_

#include <sgl-compiler.h>
#include <sgl-type.h>
#include <sgl-config.h>

#if defined(SGL_CFG_HAS_PTHREAD)
#   include "osal/posix.h"
#elif defined(SGL_CFG_HAS_WINTHREAD)
#   include "osal/windows.h"
#else
#   include "osal/dummy.h"
#endif

#endif  /* !SGL_OSAL_H_ */
