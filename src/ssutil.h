#ifndef _SSUTIL_H
#define _SSUTIL_H

#if defined(__cplusplus)
#define __SSUTIL_CLINKAGEBEGIN extern "C" {
#define __SSUTIL_CLINKAGEEND }
#else 
#define __SSUTIL_CLINKAGEBEGIN
#define __SSUTIL_CLINKAGEEND
#endif
__SSUTIL_CLINKAGEBEGIN

#include <assert.h>
#include <stdlib.h>

/* allocation */

#define SSMALLOC(SS_res, SS_size) \
  do {                            \
    (SS_res) = malloc(SS_size);   \
  } while (false)

#define SSFREE(SS_ptr)                          \
  do {                                          \
    free(SS_ptr);                               \
  } while (false)

__SSUTIL_CLINKAGEEND
#endif
