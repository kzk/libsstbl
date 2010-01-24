#ifndef _SSMTBL_H
#define _SSMTBL_H

#if defined(__cplusplus)
#define __SSMTBL_CLINKAGEBEGIN extern "C" {
#define __SSMTBL_CLINKAGEEND }
#else
#define __SSMTBL_CLINKAGEBEGIN
#define __SSMTBL_CLINKAGEEND
#endif
__SSMTBL_CLINKAGEBEGIN

#include <pthread.h>
#include <tcutil.h>

typedef struct {
  TCMDB *mdb;           /* memory database */
  uint64_t msiz;        /* total size of memory database */
  uint64_t rnum;        /* total number of records */
  pthread_rwlock_t mtx; /* mutex for record */
  int ecode;            /* error code */
} SSMTBL;

/* Create an on-memory SSTable object.
   The return value is the new on-memory SSTable object.
   The object can be shared by any threads because of the internal mutex. */
SSMTBL *ssmtblnew(void);

/* Delete an on-memory hash database object.
   `tbl' specifies the on-memory SSTable object. */
void ssmtbldel(SSMTBL *tbl);

/* Get the total size of memory used in an on-memory SSTable object.
   `tbl' specifies the on-memory SSTable object.
   The return value is the total size of memory used in the table. */
uint64_t ssmtblmsiz(SSMTBL *tbl);

/* Get the number of records stored in an on-memory SSTable object.
   `tbl' specifies the on-memory SSTable object.
   The return value is the number of the records stored in the table. */
uint64_t ssmtblrnum(SSMTBL *tbl);

/* Store a record into an on-memory SSTable object.
   `tbl' specifies the on-memory hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `vbuf' specifies the pointer to the region of the value.
   `vsiz' specifies the size of the region of the value.
   If a record with the same key exists in the database, it is overwritten. */
int ssmtblput(SSMTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);

/* Retrieve a record in an on-memory SSTable object.
   `tbl' specifies the on-memory hash database object.
   `kbuf' specifies the pointer to the region of the key.
   `ksiz' specifies the size of the region of the key.
   `sp' specifies the pointer to the variable into which the size of the region of the return
   value is assigned.
   If successful, the return value is the pointer to the region of the value of the
   corresponding record. `NULL' is returned when no record corresponds.
   Because an additional zero code is appended at the end of the region of the return value,
   the return value can be treated as a character string. Because the region of the return
   value is allocated with the `malloc' call, it should be released with the `free' call when
   it is no longer in use. */
void *ssmtblget(SSMTBL *tbl, const void *kbuf, int ksiz, int *sp);

__SSMTBL_CLINKAGEEND
#endif
