#include <ssutil.h>
#include <ssmtbl.h>

SSMTBL *ssmtblnew(void) {
  SSMTBL *tbl;
  SSMALLOC(tbl, sizeof(SSMTBL));
  ssmtblclear(tbl);
  tbl->mdb = tcmdbnew();
  if (pthread_rwlock_init(&tbl->mtx, NULL) != 0)
    goto err;
  return tbl;
err:
  if (tbl) ssmtbldel(tbl);
  return NULL;
}

void ssmtblclear(SSMTBL *tbl) {
  assert(tbl);
  tbl->mdb = NULL;
  tbl->msiz = 0;
  tbl->rnum = 0;
}

void ssmtbldel(SSMTBL *tbl) {
  assert(tbl);
  if (tbl->mdb)
    tcmdbdel(tbl->mdb);
  pthread_rwlock_destroy(&tbl->mtx);
  SSFREE(tbl);
}

uint64_t ssmtblmsiz(SSMTBL *tbl) {
  uint64_t ret = 0;
  assert(tbl);
  pthread_rwlock_rdlock(&tbl->mtx);
  ret = tbl->msiz;
  pthread_rwlock_unlock(&tbl->mtx);
  return ret;
}

uint64_t ssmtblrnum(SSMTBL *tbl) {
  uint64_t ret = 0;
  assert(tbl);
  pthread_rwlock_rdlock(&tbl->mtx);
  ret = tbl->rnum;
  pthread_rwlock_unlock(&tbl->mtx);
  return ret;
}

int ssmtblput(SSMTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  assert(tbl && tbl->mdb && kbuf && ksiz >= 0 && vbuf && vsiz >= 0);
  pthread_rwlock_wrlock(&tbl->mtx);
  tcmdbput(tbl->mdb, kbuf, ksiz, vbuf, vsiz);
  tbl->msiz = tcmdbmsiz(tbl->mdb);
  tbl->rnum = tcmdbrnum(tbl->mdb);
  pthread_rwlock_unlock(&tbl->mtx);
  return 0;
}

int ssmtblget(SSMTBL *tbl, const void *kbuf, int ksiz, int *sp) {
  assert(tbl && tbl->mdb && kbuf && ksiz >= 0 && sp);
  pthread_rwlock_rdlock(&tbl->mtx);
  tcmdbget(tbl->mdb, kbuf, ksiz, sp);
  pthread_rwlock_unlock(&tbl->mtx);
  return 0;
}
