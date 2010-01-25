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

#include <pthread.h>
#include <tcutil.h>

typedef struct {
  char *path;                  /* path of table file */
  int dfd;                     /* file descriptor for data file */
  int ifd;                     /* file descriptor for index file */
  uint64_t blksiz;             /* block size */
  uint64_t rnum;               /* total number of records */
  pthread_rwlock_t mtx;        /* mutex for record */
  uint64_t curblksiz;          /* counter for splitting into blocks in append */
  char *lastappendedkey;       /* last appended key */
  uint32_t lastappendedkeysiz; /* size of last appended key */
  int omode;                   /* open mode */
  int ecode;                   /* error code */
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
int ssftblunlink(SSFTBL *tbl);

int ssftblappend(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
void *ssftblget(SSFTBL *tbl, const void *kbuf, int ksiz, int *sp);
                     
__SSFTBL_CLINKAGEEND
#endif
