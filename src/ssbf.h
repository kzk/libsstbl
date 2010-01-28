#ifndef SSBF_H_
#define SSBF_H_

#if defined(__cplusplus)
#define SSBF_CLINKAGEBEGIN extern "C" {
#define SSBF_CLINKAGEEND }
#else 
#define SSBF_CLINKAGEBEGIN
#define SSBF_CLINKAGEEND
#endif
SSBF_CLINKAGEBEGIN

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

typedef uint64_t (*ssbf_hashfunc)(const char*, uint64_t size);
typedef struct {
  int fd;
  char *map;
  uint64_t mapsiz;
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

/* Create a bloom-filter object.
   The return value is the new bloom-filter object.
   `bziz' specifies the size of the buffer used by bloom-filter.
   The object can be shared by any threads because of the internal mutex. */
SSBF *ssbfnew(uint64_t bsiz);

/* Delete a bloom-filter object.
   `bf' specifies the bloom-filter object */
void ssbfdel(SSBF *bf);

/* Open a bloom-filter object.
   `bf' specifies the bloom-filter object.
   `path' specifies the path of the bloom-filter file.
   `omode' specifies the open mode: `SSBFOREADER' as a reader, `SSBOWRITER' as a writer.
   If the mode is `SSBFOWRITER', the following may be added by bitwise-or: `SSBFOCREAT', which
   means it creates a new database if not exist.
   The return value is 0 for success, otherwise -1. */
int ssbfopen(SSBF *bf, const char *path, int omode);

/* Close a bloom-filter object.
   `bf' specifies the bloom-filter object.
   The return value is 0 for success, otherwise -1. */
int ssbfclose(SSBF *bf);

/* Add a value to the bloom-filter.
   `bf' specifies the bloom-filter object.
   `buf' specifies the pointer to the region of the value.
   `siz' specifies the size of the region of the value.
   If a record with the same key exists in the database, it is overwritten.
   The return value is 0 for success, otherwise -1. */
int ssbfadd(SSBF *bf, const void *buf, int siz);

/* Check whether value is in bloom-filter.
   `bf' specifies the bloom-filter object.
   `buf' specifies the pointer to the region of the value.
   `siz' specifies the size of the region of the value.
   The return value is 1 if it has a possibility that the value is containted in the bloom-filter,
   0 if the value is not contained in the filter (100% confidence), -1 if error occurred. */
int ssbfhas(SSBF *bf, const void *buf, int siz);

SSBF_CLINKAGEEND
#endif
