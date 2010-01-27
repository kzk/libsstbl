#ifndef _SSCOMPRESS_H
#define _SSCOMPRESS_H

#if defined(__cplusplus)
#define __SSCOMPRESS_CLINKAGEBEGIN extern "C" {
#define __SSCOMPRESS_CLINKAGEEND }
#else
#define __SSCOMPRESS_CLINKAGEBEGIN
#define __SSCOMPRESS_CLINKAGEEND
#endif
__SSCOMPRESS_CLINKAGEBEGIN

#include <pthread.h>
#include <tcutil.h>

/* lzo */
char *sscodec_lzocompress(const char *ptr, int size, int *sp);
char *sscodec_lzodecompress(const char *ptr, int size, int * sp);

__SSCOMPRESS_CLINKAGEEND
#endif
