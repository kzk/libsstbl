#include <ssutil.h>
#include <ssftbl.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* header information */
#define FTBLHEADERSIZ 256               /* size of the header */
#define FTBLMAGICDATA "HuGeTaBlEKaMoNe" /* magic string for identification */
#define FTBLBLKSIZOFF 32                /* block size */
#define FTBLRNUMOFF   36                /* number of records */
#define FTBLIDXNUM    40                /* number of index entries */
#define FTBLIDXOFF    48                /* index info offset */

/* const or default parameters */
#define FTBLFILEMODE 00644          /* permission of created files */
#define DEFBLKSIZ (64 * 1024)       /* default data block size */

/* private function prototypes */
static void ssftblclear(SSFTBL *tbl);
static int ssftblopenimpl(SSFTBL *tbl, const char *path, int oflag);
static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static int ssftbldumpheader(SSFTBL *tbl);
static int ssftblloadheader(SSFTBL *tbl);
static int ssftbldumpindex(SSFTBL *tbl);
static int ssftblloadindex(SSFTBL *tbl);
static SSFTBLIDXENT *ssftblindexupperbound(SSFTBL *tbl, const void *kbuf, int ksiz);
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
  int r = 0;
  switch (omode) {
  case SSFTBLOWRITER:
    if (ssftblopenimpl(tbl, path, O_WRONLY | O_CREAT | O_TRUNC) != 0) return -1;
    if (ssftbldumpheader(tbl) != 0) return -1;
    tbl->omode = SSFTBLOWRITER;
    tbl->path = strdup(path);
    break;
  case SSFTBLOREADER:
    if (ssftblopenimpl(tbl, path, O_RDONLY) != 0) return -1;
    if (ssftblloadheader(tbl) != 0) return -1;
    if (ssftblloadindex(tbl) != 0) return -1;
    tbl->omode = SSFTBLOREADER;
    tbl->path = strdup(path);
    break;
  default:
    r = -1;
    ssftblsetecode(tbl, SSEINVALID);
    break;
  }
  return r;
}

int ssftblclose(SSFTBL *tbl) {
  int r = 0; uint32_t i;
  if (tbl->dfd < 0) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  if (tbl->path) {
    SSFREE(tbl->path);
    tbl->path = NULL;
  }
  if (tbl->dfd >= 0) {
    if (tbl->omode == SSFTBLOWRITER && tbl->lastappended.kbuf) {
      /* record last entry into tbl->idx */
      SSFTBLIDXENT *e = &tbl->lastappended;
      off_t idxoff = lseek(tbl->dfd, 0, SEEK_END);
      if (idxoff == -1) {
        ssftblsetecode(tbl, SSESEEK);
        r = -1;
      }
      tbl->idxoff = (uint64_t)idxoff;
      tbl->idxnum++;
      SSREALLOC(tbl->idx, tbl->idx, sizeof(SSFTBLIDXENT) * tbl->idxnum);
      SSMALLOC(tbl->idx[tbl->idxnum-1].kbuf, e->ksiz);
      memcpy(tbl->idx[tbl->idxnum-1].kbuf, e->kbuf, e->ksiz);
      tbl->idx[tbl->idxnum-1].ksiz = e->ksiz;
      tbl->idx[tbl->idxnum-1].doff = e->doff;
      /* dump header & index */
      /* TODO: fix error path */
      if (r == 0) ssftbldumpheader(tbl);
      if (r == 0) ssftbldumpindex(tbl);
    }
    SSSYS_NOINTR(r, fsync(tbl->dfd));
    SSSYS_NOINTR(r, close(tbl->dfd));
    tbl->dfd = -1;
  }
  tbl->curblksiz = 0;
  if (tbl->lastappended.kbuf) {
    SSFREE(tbl->lastappended.kbuf);
    tbl->lastappended.kbuf = NULL;
    tbl->lastappended.ksiz = 0;
    tbl->lastappended.doff = 0;
  }
  if (tbl->idx) {
    for (i = 0; i < tbl->idxnum; i++)
      SSFREE(tbl->idx[i].kbuf);
    SSFREE(tbl->idx);
    tbl->idx = NULL;
    tbl->idxnum = 0;
  }
  return r;
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
  if (tbl->idxnum == 0) return NULL;
  SSFTBLIDXENT *ubound = ssftblindexupperbound(tbl, kbuf, ksiz);
  SSFTBLIDXENT *first = tbl->idx;
  SSFTBLIDXENT *last = tbl->idx + tbl->idxnum;
  assert(first <= ubound && ubound <= last);
  if (ubound == first)
    return NULL;
  if (ubound == last && FTKEYCMPGREATER(kbuf, ksiz, (last-1)->kbuf, (last-1)->ksiz))
    return NULL;
  SSFTBLIDXENT *e = ubound - 1;
  assert(first <= e && e < last);
  return ssftblgetbyscan(tbl, e, kbuf, ksiz, sp);
}

/*-----------------------------------------------------------------------------
 * private functions
 */
static void ssftblclear(SSFTBL *tbl) {
  assert(tbl);
  tbl->path = NULL;
  tbl->dfd = -1;
  tbl->blksiz = DEFBLKSIZ;
  tbl->rnum = 0;
  tbl->omode = 0;
  tbl->ecode = SSESUCCESS;
  tbl->curblksiz = 0;
  tbl->lastappended.kbuf = NULL;
  tbl->lastappended.ksiz = 0;
  tbl->lastappended.doff = 0;
  tbl->idx = NULL;
  tbl->idxnum = 0;
}

static int ssftblopenimpl(SSFTBL *tbl, const char *basepath, int oflag) {
  int r, fd;
  char *path;
  SSMALLOC(path, strlen(basepath) + strlen(".sstbl"));
  sprintf(path, "%s.sstbl", basepath);
  SSSYS_NOINTR(fd, open(path, oflag, FTBLFILEMODE));
  if (fd < 0) {
    int ecode = SSEOPEN;
    switch (errno) {
    case EACCES: ecode = SSENOPERM; break;
    case ENOENT: ecode = SSENOFILE; break;
    case ENOTDIR: ecode = SSENOFILE; break;
    }
    ssftblsetecode(tbl, ecode);
    goto err;
  }
  tbl->dfd = fd;
  return 0;
err:
  SSSYS_NOINTR(r, close(fd));
  return -1;
}

static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  uint64_t doff = lseek(tbl->dfd, 0, SEEK_END);
  if (tbl->lastappended.kbuf == NULL || tbl->curblksiz >= tbl->blksiz) {
    tbl->idxnum++;
    /* record the index entry */
    SSREALLOC(tbl->idx, tbl->idx, sizeof(SSFTBLIDXENT) * tbl->idxnum);
    SSMALLOC(tbl->idx[tbl->idxnum-1].kbuf, ksiz);
    memcpy(tbl->idx[tbl->idxnum-1].kbuf, kbuf, ksiz);
    tbl->idx[tbl->idxnum-1].ksiz = ksiz;
    tbl->idx[tbl->idxnum-1].doff = doff;
    /* move to the next block */
    tbl->curblksiz = 0;
  }
  if (sswrite(tbl->dfd, &ksiz, sizeof(ksiz)) != 0) goto err;
  if (sswrite(tbl->dfd, kbuf, ksiz) != 0) goto err;
  if (sswrite(tbl->dfd, &vsiz, sizeof(vsiz)) != 0) goto err;
  if (sswrite(tbl->dfd, vbuf, vsiz) != 0) goto err;
  tbl->curblksiz += vsiz;
  assert(tbl->blksiz > 0);
  SSREALLOC(tbl->lastappended.kbuf, tbl->lastappended.kbuf, ksiz);
  memcpy(tbl->lastappended.kbuf, kbuf, ksiz);
  tbl->lastappended.ksiz = ksiz;
  tbl->lastappended.doff = doff;
  return 0;
err:
  ssftblsetecode(tbl, SSEWRITE);
  return -1;
}

static int ssftbldumpheader(SSFTBL *tbl) {
  assert(tbl);
  char buf[FTBLHEADERSIZ];
  memset(buf, 0, FTBLHEADERSIZ);
  sprintf(buf, "%s\n", FTBLMAGICDATA);
  memcpy(buf + FTBLBLKSIZOFF, &tbl->blksiz, sizeof(tbl->blksiz));
  memcpy(buf + FTBLRNUMOFF, &tbl->rnum, sizeof(tbl->rnum));
  memcpy(buf + FTBLIDXNUM, &tbl->idxnum, sizeof(tbl->idxnum));
  memcpy(buf + FTBLIDXOFF, &tbl->idxoff, sizeof(tbl->idxoff));
  if (lseek(tbl->dfd, 0, SEEK_SET) != 0) {
    ssftblsetecode(tbl, SSESEEK);
    return -1;
  }
  if (sswrite(tbl->dfd, buf, FTBLHEADERSIZ) != 0) {
    ssftblsetecode(tbl, SSEWRITE);
    return -1;
  }
  return 0;
}

static int ssftblloadheader(SSFTBL *tbl) {
  char buf[FTBLHEADERSIZ];
  if (lseek(tbl->dfd, 0, SEEK_SET) != 0) {
    ssftblsetecode(tbl, SSESEEK);
    return -1;
  }
  if (ssread(tbl->dfd, buf, FTBLHEADERSIZ) != 0) {
    ssftblsetecode(tbl, SSEREAD);
    return -1;
  }
  memcpy(&tbl->blksiz, buf + FTBLBLKSIZOFF, sizeof(tbl->blksiz));
  memcpy(&tbl->rnum,   buf + FTBLRNUMOFF, sizeof(tbl->rnum));
  memcpy(&tbl->idxnum, buf + FTBLIDXNUM, sizeof(tbl->idxnum));
  memcpy(&tbl->idxoff, buf + FTBLIDXOFF, sizeof(tbl->idxoff));
  assert(tbl);
  return 0;
}

static int ssftbldumpindex(SSFTBL *tbl) {
  assert(tbl);
  assert(tbl->idxoff >= FTBLHEADERSIZ);
  if (tbl->dfd < 0) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  off_t idxoff = lseek(tbl->dfd, 0, SEEK_END);
  if (idxoff == -1) {
    ssftblsetecode(tbl, SSESEEK);
    return -1;
  }
  uint32_t i;
  for (i = 0; i < tbl->idxnum; i++) {
    SSFTBLIDXENT *e = tbl->idx + i;
    if (sswrite(tbl->dfd, &e->ksiz, sizeof(e->ksiz)) != 0) return -1;
    if (sswrite(tbl->dfd, e->kbuf,  e->ksiz)         != 0) return -1;
    if (sswrite(tbl->dfd, &e->doff, sizeof(e->doff)) != 0) return -1;
  }
  return 0;
}

static int ssftblloadindex(SSFTBL *tbl) {
  assert(tbl);
  if (lseek(tbl->dfd, tbl->idxoff, SEEK_SET) != (off_t)tbl->idxoff) {
    ssftblsetecode(tbl, SSESEEK);
    return -1;
  }
  SSMALLOC(tbl->idx, sizeof(SSFTBLIDXENT) * tbl->idxnum);
  uint32_t i;
  for (i = 0; i < tbl->idxnum; i++) {
    SSFTBLIDXENT *e = tbl->idx + i;
    if (ssread(tbl->dfd, &e->ksiz, sizeof(e->ksiz)) != 0) return -1;
    SSMALLOC(e->kbuf, e->ksiz);
    if (ssread(tbl->dfd, e->kbuf,  e->ksiz)         != 0) return -1;
    if (ssread(tbl->dfd, &e->doff, sizeof(e->doff)) != 0) return -1;
  }
  return 0;
}

static SSFTBLIDXENT *ssftblindexupperbound(SSFTBL *tbl, const void *kbuf, int ksiz) {
  assert(tbl && kbuf && ksiz);
  uint32_t len = tbl->idxnum;
  uint32_t half;
  SSFTBLIDXENT *first, *last, *middle;
  first = tbl->idx;
  last = tbl->idx + len;
  while (len > 0) {
    half = len >> 1;
    middle = first + half;
    if (FTKEYCMPLESS(kbuf, ksiz, middle->kbuf, middle->ksiz)) {
      len = half;
    } else {
      first = middle;
      first++;
      len = len - half - 1;
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
