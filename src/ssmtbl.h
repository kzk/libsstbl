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

typedef struct {

} SSMTBL;

SSMTBL *ssmtblnew();
void ssmtbldel(SSMTBL *tbl);

int ssmtblopen(SSMTBL *tbl, const char *path, int omode);
int ssmtblclose(SSMTBL *tbl);

int ssmtblput(SSMTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
int ssmtblget(SSMTBL *tbl, const void *kbuf, int ksiz, int *sp);

__SSMTBL_CLINKAGEEND
#endif
