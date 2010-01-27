#ifndef _SSFTBL_H
#define _SSFTBL_H

#if defined(__cplusplus)
#define __SSFTBL_CLINKAGEBEGIN extern "C" {
#define __SSFTBL_CLINKAGEEND }
#else
#define __SSFTBL_CLINKAGEBEGIN
#define __SSFTBL_CLINKAGEEND
#endif
__SSFTBL_CLINKAGEBEGIN

#include <ssutil.h>

typedef struct {
  char *kbuf;      /* key data */
  int ksiz;        /* key size */
  uint64_t doff;   /* offset in data file */
  uint32_t blksiz; /* size of block in data file */
} SSFTBLIDXENT;

typedef struct {
  char *path;                  /* path of table file */
  int dfd;                     /* file descriptor for data file */
  uint32_t blksiz;             /* block size */
  uint32_t rnum;               /* total number of records */
  pthread_rwlock_t mtx;        /* mutex for record */
  int omode;                   /* open mode */
  int ecode;                   /* error code */
  /* writer-only */
  char *blkbuf;                /* block buffer */
  uint32_t blkbufsiz;          /* size of block buffer */
  uint32_t curblkrnum;         /* number of records in the current block */
  uint32_t curblksiz;          /* counter for splitting into blocks in append */
  SSFTBLIDXENT lastappended;   /* last appended key info */
  /* reader-only */
  SSFTBLIDXENT *idx;           /* index used for binary-search */
  uint32_t idxnum;             /* number of index entry */
  uint64_t idxoff;             /* offset to the index file */
} SSFTBL;

enum SSFTBLOMODE { /* enumeration for open modes */
  SSFTBLOREADER = 1 << 0, /* open as a reader */
  SSFTBLOWRITER = 1 << 1  /* open as a writer */
};

SSFTBL *ssftblnew(void);
void ssftbldel(SSFTBL *tbl);
int ssftbltune(SSFTBL *tbl, uint64_t blksiz);

int ssftblopen(SSFTBL *tbl, const char *path, enum SSFTBLOMODE omode);
int ssftblclose(SSFTBL *tbl);

int ssftblappend(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
void *ssftblget(SSFTBL *tbl, const void *kbuf, int ksiz, int *sp);
                     
__SSFTBL_CLINKAGEEND
#endif
