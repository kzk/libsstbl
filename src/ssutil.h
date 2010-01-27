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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

/* error codes */
enum {                                   /* enumeration for error codes */
  SSESUCCESS,                            /* success */
  SSETHREAD,                             /* threading error */
  SSEINVALID,                            /* invalid operation */
  SSENOFILE,                             /* file not found */
  SSENOPERM,                             /* no permission */
  SSEMETA,                               /* invalid meta data */
  SSERHEAD,                              /* invalid record header */
  SSEOPEN,                               /* open error */
  SSECLOSE,                              /* close error */
  SSETRUNC,                              /* trunc error */
  SSESYNC,                               /* sync error */
  SSESTAT,                               /* stat error */
  SSESEEK,                               /* seek error */
  SSEREAD,                               /* read error */
  SSEWRITE,                              /* write error */
  SSEMMAP,                               /* mmap error */
  SSELOCK,                               /* lock error */
  SSEUNLINK,                             /* unlink error */
  SSERENAME,                             /* rename error */
  SSEMKDIR,                              /* mkdir error */
  SSERMDIR,                              /* rmdir error */
  SSEKEEP,                               /* existing record */
  SSENOREC,                              /* no record found */
  SSEMISC = 9999                         /* miscellaneous error */
};

/* allocation */
#define SSMALLOC(SS_res, SS_size) \
  do {                            \
    (SS_res) = malloc(SS_size);   \
  } while (0)

#define SSREALLOC(SS_res, SS_ptr, SS_size)       \
  do {                                           \
    (SS_res) = realloc(SS_ptr, SS_size);         \
  } while (0)  

#define SSFREE(SS_ptr)                          \
  do {                                          \
    free(SS_ptr);                               \
  } while (0)

/* system calls */
#define SSSYS_NOINTR(result, expr)              \
  do {						\
    (result) = (expr);				\
    if ((result) < 0 && errno == EINTR)		\
      continue;					\
    break;					\
  } while (1)

/* I/O */
int sswrite(int fd, const void *buf, size_t size);
int ssread(int fd, void *buf, size_t size);

__SSUTIL_CLINKAGEEND
#endif
