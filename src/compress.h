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

#define HAVE_ZLIB 1
#define HAVE_LZO  0 /* disabled */

/* none */
char *sscodec_nonecompress(const char *ptr, int size, int *sp);
char *sscodec_nonedecompress(const char *ptr, int size, int *sp);

/* zlib */
char *sscodec_zlibcompress(const char *ptr, int size, int *sp);
char *sscodec_zlibdecompress(const char *ptr, int size, int * sp);

/* lzo */
char *sscodec_lzocompress(const char *ptr, int size, int *sp);
char *sscodec_lzodecompress(const char *ptr, int size, int * sp);

__SSCOMPRESS_CLINKAGEEND
#endif
