#ifndef _SSBF_H
#define _SSBF_H

#if defined(__cplusplus)
#define __SSBF_CLINKAGEBEGIN extern "C" {
#define __SSBF_CLINKAGEEND }
#else 
#define __SSBF_CLINKAGEBEGIN
#define __SSBF_CLINKAGEEND
#endif
__SSBF_CLINKAGEBEGIN

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

typedef uint64_t (*ssbf_hashfunc)(const char*, uint64_t size);
typedef struct {
  int fd;
  char *b;
  uint64_t bsiz;
  uint32_t omode;
  uint64_t nfuncs;
  ssbf_hashfunc *funcs;
  pthread_rwlock_t mtx;
  int ecode;
} SSBF;

enum { /* enumeration for open modes */
  SSBFOREADER = 1 << 0, /* open as a reader */
  SSBFOWRITER = 1 << 1, /* open as a writer */
  SSBFOCREAT  = 1 << 2, /* writer creating */
};

SSBF *ssbfnew(uint64_t bsiz);
void ssbfdel(SSBF *bf);

int ssbfopen(SSBF *bf, const char *path, int omode);
int ssbfclose(SSBF *bf);

int ssbfadd(SSBF *bf, const void *buf, int siz);
int ssbfhas(SSBF *bf, const void *buf, int siz);

__SSBF_CLINKAGEEND
#endif
