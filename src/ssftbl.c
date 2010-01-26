#include <ssutil.h>
#include <ssftbl.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define SSFTBLBLOCKSIZ (64 * 1024)

/* private function prototypes */
static void ssftblclear(SSFTBL *tbl);
static int ssftblopenimpl(SSFTBL *tbl, const char *path, int oflag);
static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static int ssftblloadindex(SSFTBL *tbl);
static SSFTBLIDXENT *ssftblindexlowerbound(SSFTBL *tbl, const void *kbuf, int ksiz);
static void *ssftblgetbyscan(SSFTBL *tbl, SSFTBLIDXENT *e, const void *kbuf, int ksiz, int *sp);
static int ssftblkeycmp(const char *s1, size_t n1, const char *s2, size_t n2);
static void ssftblsetecode(SSFTBL *tbl, int ecode);

/* private macros */
#define FTKEYCMPGREATER(s1, n1, s2, n2) (ssftblkeycmp(s1, n1, s2, n2)  > 0)
#define FTKEYCMPEQUAL(s1, n1, s2, n2)   (ssftblkeycmp(s1, n1, s2, n2) == 0)
#define FTKEYCMPLESS(s1, n1, s2, n2)    (ssftblkeycmp(s1, n1, s2, n2)  < 0)

/*-----------------------------------------------------------------------------
 * APIs
 */
SSFTBL *ssftblnew(void) {
  SSFTBL *tbl = NULL;
  SSMALLOC(tbl, sizeof(SSFTBL));
  ssftblclear(tbl);
  return tbl;
}

void ssftbldel(SSFTBL *tbl) {
  assert(tbl);
  if (tbl->dfd >= 0)
    ssftblclose(tbl);
  SSFREE(tbl);
}

int ssftbltune(SSFTBL *tbl, uint64_t blksiz) {
  assert(tbl);
  tbl->blksiz = blksiz;
  return 0;
}

int ssftblopen(SSFTBL *tbl, const char *path, enum SSFTBLOMODE omode) {
  if (tbl->dfd >= 0) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  int r;
  switch (omode) {
  case SSFTBLOWRITER:
    r = ssftblopenimpl(tbl, path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND);
    if (r == 0) tbl->omode = SSFTBLOWRITER;
    break;
  case SSFTBLOREADER:
    r = ssftblopenimpl(tbl, path, O_RDONLY);
    if (r == 0) r = ssftblloadindex(tbl);
    if (r == 0) tbl->omode = SSFTBLOREADER;
    break;
  default:
    r = -1;
    ssftblsetecode(tbl, SSEINVALID);
    break;
  }
  if (r == 0) tbl->path = strdup(path);
  return r;
}

int ssftblclose(SSFTBL *tbl) {
  int r = 0;
  if (tbl->dfd < 0) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  if (tbl->path) {
    SSFREE(tbl->path);
    tbl->path = NULL;
  }
  if (tbl->dfd >= 0) {
    SSSYS_NOINTR(r, fsync(tbl->dfd));
    SSSYS_NOINTR(r, close(tbl->dfd));
    tbl->dfd = -1;
  }
  if (tbl->ifd >= 0) {
    if (tbl->omode == SSFTBLOWRITER && tbl->lastappended.kbuf) {
      /* record last entry into index file */
      SSFTBLIDXENT *e = &tbl->lastappended;
      if (sswrite(tbl->ifd, &e->ksiz, sizeof(e->ksiz)) != 0) r = -1;
      if (sswrite(tbl->ifd, e->kbuf,  e->ksiz)         != 0) r = -1;
      if (sswrite(tbl->ifd, &e->doff, sizeof(e->doff)) != 0) r = -1;
    }
    SSSYS_NOINTR(r, fsync(tbl->dfd));
    SSSYS_NOINTR(r, close(tbl->ifd));
    tbl->ifd = -1;
  }
  tbl->curblksiz = 0;
  if (tbl->lastappended.kbuf) {
    SSFREE(tbl->lastappended.kbuf);
    tbl->lastappended.kbuf = NULL;
    tbl->lastappended.ksiz = 0;
    tbl->lastappended.doff = 0;
  }
  if (tbl->idx) {
    SSFREE(tbl->idx);
    tbl->idx = NULL;
    tbl->idxsiz = 0;
  }
  return 0;
}

int ssftblappend(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  assert(tbl && kbuf && ksiz > 0 && vbuf && vsiz > 0);
  if (tbl->omode != SSFTBLOWRITER) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  if (tbl->lastappended.kbuf != NULL) {
    if (FTKEYCMPGREATER(tbl->lastappended.kbuf, tbl->lastappended.ksiz, kbuf, ksiz)) {
      /* keys must be appended in the lexicographically ascending order */
      ssftblsetecode(tbl, SSEINVALID);
      return -1;
    }
  }
  return ssftblappendimpl(tbl, kbuf, ksiz, vbuf, vsiz);
}

void *ssftblget(SSFTBL *tbl, const void *kbuf, int ksiz, int *sp) {
  if (tbl->idxsiz == 0) return NULL;
  SSFTBLIDXENT *lbound = ssftblindexlowerbound(tbl, kbuf, ksiz);
  SSFTBLIDXENT *first = tbl->idx;
  SSFTBLIDXENT *last = tbl->idx + tbl->idxsiz;
  if (lbound == first && FTKEYCMPLESS(kbuf, ksiz, first->kbuf, first->ksiz))
    return NULL;
  if (lbound == last)
    return NULL;
  SSFTBLIDXENT *e = lbound - 1;
  if (lbound == first && FTKEYCMPEQUAL(kbuf, ksiz, first->kbuf, first->ksiz))
    e = first;
  return ssftblgetbyscan(tbl, e, kbuf, ksiz, sp);
}

/*-----------------------------------------------------------------------------
 * private functions
 */
static void ssftblclear(SSFTBL *tbl) {
  assert(tbl);
  tbl->path = NULL;
  tbl->dfd = -1;
  tbl->ifd = -1;
  tbl->blksiz = SSFTBLBLOCKSIZ;
  tbl->rnum = 0;
  tbl->omode = 0;
  tbl->ecode = SSESUCCESS;
  tbl->curblksiz = 0;
  tbl->lastappended.kbuf = NULL;
  tbl->lastappended.ksiz = 0;
  tbl->lastappended.doff = 0;
  tbl->idx = NULL;
  tbl->idxsiz = 0;
}

static int ssftblopenimpl(SSFTBL *tbl, const char *path, int oflag) {
  int r, i;
  int fds[2]; /* fd[0] -> dfd, fd[1] -> ifd */
  char *paths[2];
  SSMALLOC(paths[0], strlen(path) + strlen(".sstbld") + 1);
  SSMALLOC(paths[1], strlen(path) + strlen(".sstbli") + 1);
  sprintf(paths[0], "%s.sstbld", path);
  sprintf(paths[1], "%s.sstbli", path);
  for (i = 0; i < 2; i++) {
    int fd;
    SSSYS_NOINTR(fd, open(paths[i], oflag, 0644));
    if (fd < 0) {
      perror("error");
      int ecode = SSEOPEN;
      switch (errno) {
      case EACCES: ecode = SSENOPERM; break;
      case ENOENT: ecode = SSENOFILE; break;
      case ENOTDIR: ecode = SSENOFILE; break;
      }
      ssftblsetecode(tbl, ecode);
      goto err;
    }
    fds[i] = fd;
  }
  tbl->dfd = fds[0];
  tbl->ifd = fds[1];
  return 0;
err:
  for (i = 0; i < 2; i++)
    if (fds[i] >= 0)
      SSSYS_NOINTR(r, close(fds[i]));
  return -1;
}

static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  uint64_t doff = lseek(tbl->dfd, 0, SEEK_END);
  if (sswrite(tbl->dfd, &ksiz, sizeof(ksiz)) != 0) goto err;
  if (sswrite(tbl->dfd, kbuf, ksiz) != 0) goto err;
  if (sswrite(tbl->dfd, &vsiz, sizeof(vsiz)) != 0) goto err;
  if (sswrite(tbl->dfd, vbuf, vsiz) != 0) goto err;
  tbl->curblksiz += vsiz;
  assert(tbl->blksiz > 0);
  if (tbl->lastappended.kbuf == NULL || tbl->curblksiz >= tbl->blksiz) {
    /* record into index file */
    if (sswrite(tbl->ifd, &ksiz, sizeof(ksiz)) != 0) goto err;
    if (sswrite(tbl->ifd, kbuf,  ksiz)         != 0) goto err;
    if (sswrite(tbl->ifd, &doff, sizeof(doff)) != 0) goto err;
    /* move to the next block */
    tbl->curblksiz = 0;
  }
  SSREALLOC(tbl->lastappended.kbuf, tbl->lastappended.kbuf, ksiz);
  memcpy(tbl->lastappended.kbuf, kbuf, ksiz);
  tbl->lastappended.ksiz = ksiz;
  tbl->lastappended.doff = doff;
  return 0;
err:
  ssftblsetecode(tbl, SSEWRITE);
  return -1;
}

static int ssftblloadindex(SSFTBL *tbl) {
  assert(tbl);
  if (tbl->ifd < 0) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  uint32_t n = 0;
  while (1) {
    // TODO: proper file termination condition
    // TODO: remove realloc
    int ksiz; char *kbuf; uint64_t doff;
    if (ssread(tbl->ifd, &ksiz, sizeof(int)) < 0) break;
    SSMALLOC(kbuf, ksiz);
    if (ssread(tbl->ifd, kbuf, ksiz) < 0) break;
    if (ssread(tbl->ifd, &doff, sizeof(uint64_t)) < 0) break;
    n++;
    SSREALLOC(tbl->idx, tbl->idx, sizeof(tbl->idx[0]) * n);
    tbl->idx[n-1].kbuf = kbuf;
    tbl->idx[n-1].ksiz = ksiz;
    tbl->idx[n-1].doff = doff;
  }
  tbl->idxsiz = n;
  return 0;
}

static SSFTBLIDXENT *ssftblindexlowerbound(SSFTBL *tbl, const void *kbuf, int ksiz) {
  assert(tbl && kbuf && ksiz);
  uint32_t len = tbl->idxsiz;
  uint32_t half;
  SSFTBLIDXENT *first, *last, *middle;
  first = tbl->idx;
  last = tbl->idx + len;
  while (len > 0) {
    half = len >> 1;
    middle = first + half;
    if (FTKEYCMPLESS(middle->kbuf, middle->ksiz, kbuf, ksiz)) {
      first = middle;
      first++;
      len = len - half - 1;
    } else {
      len = half;
    }
  }
  return first;
}

static void *ssftblgetbyscan(SSFTBL *tbl, SSFTBLIDXENT *e, const void *kb, int ks, int *sp) {
  assert(e && kb && ks);
  // TODO: mmap is more faster?
  int r;
  int dfd = dup(tbl->dfd);
  if (lseek(dfd, e->doff, SEEK_SET) != (off_t)e->doff) {
    ssftblsetecode(tbl, SSESEEK);
    SSSYS_NOINTR(r, close(dfd));
    return NULL;
  }
  // TODO: posix_fadvise
  uint64_t curblksiz = 0;
  int ksiz; char *kbuf = NULL; int vsiz; char *vbuf = NULL;
  while (1) {
    /* read key */
    if (ssread(dfd, &ksiz, sizeof(ksiz)) < 0) break;
    SSREALLOC(kbuf, kbuf, ksiz);
    if (ssread(dfd, kbuf, ksiz) < 0) break;
    /* read val */
    if (ssread(dfd, &vsiz, sizeof(vsiz)) < 0) break;
    SSREALLOC(vbuf, vbuf, vsiz);
    if (ssread(dfd, vbuf, vsiz) < 0) break;
    /* compare */
    if (FTKEYCMPEQUAL(kbuf, ksiz, kb, ks)) {
      if (sp) *sp = vsiz;
      SSFREE(kbuf);
      SSSYS_NOINTR(r, close(dfd));
      return vbuf;
    }
    curblksiz += vsiz;
    if (curblksiz >= tbl->blksiz)
      break;
  }
  if (kbuf) SSFREE(kbuf);
  if (vbuf) SSFREE(vbuf);
  SSSYS_NOINTR(r, close(dfd));
  return NULL;
}

static int ssftblkeycmp(const char *s1, size_t n1, const char *s2, size_t n2) {
  /* same with std::string ordering */
  assert(s1 && s2);
  size_t min = (n1 < n2) ? n1 : n2;
  int r = memcmp(s1, s2, min);
  if (r == 0) {
    if (n1 == n2) return 0;
    return (n1 < n2) ? -1 : 1;
  }
  return r;
}

static void ssftblsetecode(SSFTBL *tbl, int ecode) {
  assert(tbl);
  tbl->ecode = ecode;
}
