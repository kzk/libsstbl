#include <ssutil.h>
#include <ssftbl.h>

#include <tcutil.h> /* for tcmdb */

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
#define FTBLFILEMODE   00644            /* permission of created files */
#define FTBLFILESUFFIX ".sstbl"         /* suffix of data file */
#define DEFBLKSIZ      (64 * 1024)      /* default data block size */
#define DEFBLKCNUM     (4 * 1024)       /* default cache entry num */
#define FTBLBLKCOUT    256

/* private function prototypes */
static void ssftblclear(SSFTBL *tbl);
static int ssftblopenimpl(SSFTBL *tbl, const char *path, int oflag);
static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static int ssftbldumpheader(SSFTBL *tbl);
static int ssftblloadheader(SSFTBL *tbl);
static int ssftbldumpindex(SSFTBL *tbl);
static int ssftblloadindex(SSFTBL *tbl);
static int ssftbldumpblk(SSFTBL *tbl, int fd, char *buf, int bufsiz, int *sp);
static char *ssftblloadblk(SSFTBL *tbl, int fd, uint64_t doff, int blksiz, int *sp);
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

int ssftblsetcache(SSFTBL *tbl, uint32_t blkcnum) {
  assert(tbl);
  tbl->blkcnum = (blkcnum > 0) ? blkcnum : 1;
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
    tbl->blkc = tcmdbnew2(tbl->blkcnum * 2 + 1);
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
      int err = 0;
      /* write current blkbuf */
      int blksiz = 0;
      uint64_t doff = 0;
      if (tbl->curblksiz > 0) {
        assert(tbl->curblkrnum > 0);
        tbl->idx[tbl->idxnum-1].blksiz = tbl->curblksiz;
        if (tbl->curblkrnum == 1) {
          doff = (uint64_t)lseek(tbl->dfd, 0, SEEK_END); /* new block */
        } else if (tbl->curblkrnum > 1) {
          doff = tbl->idx[tbl->idxnum-1].doff; /* same with last index block */
        }
      }
      if (ssftbldumpblk(tbl, tbl->dfd, tbl->blkbuf, tbl->curblksiz, &blksiz) != 0)
        err = -1;
      /* record last entry into tbl->idx */
      SSFTBLIDXENT *e = &tbl->lastappended;
      tbl->idxnum++;
      SSREALLOC(tbl->idx, tbl->idx, sizeof(SSFTBLIDXENT) * tbl->idxnum);
      SSMALLOC(tbl->idx[tbl->idxnum-1].kbuf, e->ksiz);
      memcpy(tbl->idx[tbl->idxnum-1].kbuf, e->kbuf, e->ksiz);
      tbl->idx[tbl->idxnum-1].ksiz = e->ksiz;
      tbl->idx[tbl->idxnum-1].doff = doff;
      tbl->idx[tbl->idxnum-1].blksiz = blksiz;
      /* dump header */
      if (ssftbldumpheader(tbl) != 0) err = -1;
      /* dump index */
      off_t idxoff = lseek(tbl->dfd, 0, SEEK_END);
      if (idxoff == -1) {
        ssftblsetecode(tbl, SSESEEK);
        err = -1;
      }
      tbl->idxoff = (uint64_t)idxoff;
      if (ssftbldumpindex(tbl) != 0) err = -1;
      r = err;
    }
    SSSYS_NOINTR(r, fsync(tbl->dfd));
    SSSYS_NOINTR(r, close(tbl->dfd));
    tbl->dfd = -1;
  }
  if (tbl->blkbuf) {
    SSFREE(tbl->blkbuf);
    tbl->blkbuf = NULL;
  }
  tbl->blkbufsiz = 0;
  tbl->curblkrnum = 0;
  tbl->curblksiz = 0;
  if (tbl->lastappended.kbuf) {
    SSFREE(tbl->lastappended.kbuf);
    tbl->lastappended.kbuf = NULL;
    tbl->lastappended.ksiz = 0;
    tbl->lastappended.doff = 0;
    tbl->lastappended.blksiz = 0;
  }
  if (tbl->idx) {
    for (i = 0; i < tbl->idxnum; i++)
      SSFREE(tbl->idx[i].kbuf);
    SSFREE(tbl->idx);
    tbl->idx = NULL;
    tbl->idxnum = 0;
  }
  if (tbl->blkc) {
    tcmdbdel(tbl->blkc);
    tbl->blkc = NULL;
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

void *ssftblgetfirstkey(SSFTBL *tbl, int *sp) {
  assert(tbl && sp);
  if (tbl->dfd < 0 || tbl->omode != SSFTBLOREADER) {
    ssftblsetecode(tbl, SSEINVALID);
    return NULL;
  }
  char *ret;
  SSFTBLIDXENT *e = tbl->idx + 0;
  SSMALLOC(ret, e->ksiz);
  memcpy(ret, e->kbuf, e->ksiz);
  *sp = e->ksiz;
  return ret;
}

void *ssftblgetlastkey(SSFTBL *tbl, int *sp) {
  if (tbl->dfd < 0 || tbl->omode != SSFTBLOREADER) {
    ssftblsetecode(tbl, SSEINVALID);
    return NULL;
  }
  assert(tbl->idxnum >= 2);
  char *ret;
  SSFTBLIDXENT *e = tbl->idx + tbl->idxnum - 1;
  SSMALLOC(ret, e->ksiz);
  memcpy(ret, e->kbuf, e->ksiz);
  *sp = e->ksiz;
  return ret;
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
  tbl->blkbuf = NULL;
  tbl->blkbufsiz = 0;
  tbl->curblkrnum = 0;
  tbl->curblksiz = 0;
  tbl->lastappended.kbuf = NULL;
  tbl->lastappended.ksiz = 0;
  tbl->lastappended.doff = 0;
  tbl->lastappended.blksiz = 0;
  tbl->idx = NULL;
  tbl->idxnum = 0;
  tbl->idxoff = 0;
  tbl->blkc = NULL;
  tbl->blkcnum = DEFBLKCNUM;
}

static int ssftblopenimpl(SSFTBL *tbl, const char *basepath, int oflag) {
  int r, fd;
  char *path;
  SSMALLOC(path, strlen(basepath) + strlen(FTBLFILESUFFIX));
  sprintf(path, "%s%s", basepath, FTBLFILESUFFIX);
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
  assert(tbl->blksiz > 0);
  int isfirstappend = (tbl->lastappended.kbuf == NULL);
  int ismovetonext = (tbl->curblksiz >= tbl->blksiz);
  if (isfirstappend || ismovetonext) {
    /* write current blkbuf */
    uint64_t doff = 0;
    int blksiz = 0;
    if (isfirstappend) {
      doff = lseek(tbl->dfd, 0, SEEK_END);
    } else if (ismovetonext) {
      if (ssftbldumpblk(tbl, tbl->dfd, tbl->blkbuf, tbl->curblksiz, &blksiz) != 0)
        return -1;
      tbl->idx[tbl->idxnum-1].blksiz = blksiz;
      doff = lseek(tbl->dfd, 0, SEEK_END);
    }
    tbl->idxnum++;
    /* record the index entry */
    SSREALLOC(tbl->idx, tbl->idx, sizeof(SSFTBLIDXENT) * tbl->idxnum);
    SSMALLOC(tbl->idx[tbl->idxnum-1].kbuf, ksiz);
    memcpy(tbl->idx[tbl->idxnum-1].kbuf, kbuf, ksiz);
    tbl->idx[tbl->idxnum-1].ksiz = ksiz;
    tbl->idx[tbl->idxnum-1].doff = doff;
    /* move to the next block */
    SSREALLOC(tbl->blkbuf, tbl->blkbuf, tbl->blksiz);
    tbl->blkbufsiz = tbl->blksiz;
    tbl->curblkrnum = 0;
    tbl->curblksiz = 0;
  }
  /* append to blkbuf */
  uint32_t datasiz = sizeof(ksiz) + ksiz + sizeof(vsiz) + vsiz;
  if (tbl->blkbufsiz < tbl->curblksiz + datasiz) {
    SSREALLOC(tbl->blkbuf, tbl->blkbuf, tbl->curblksiz + datasiz);
    tbl->blkbufsiz = tbl->curblksiz + datasiz;
  }
  char *p = tbl->blkbuf + tbl->curblksiz;
  memcpy(p, &ksiz, sizeof(ksiz));
  p += sizeof(ksiz);
  memcpy(p, kbuf, ksiz);
  p += ksiz;
  memcpy(p, &vsiz, sizeof(vsiz));
  p += sizeof(vsiz);
  memcpy(p, vbuf, vsiz);
  /* update lastappended */
  tbl->curblkrnum++;
  tbl->curblksiz += datasiz;
  SSREALLOC(tbl->lastappended.kbuf, tbl->lastappended.kbuf, ksiz);
  memcpy(tbl->lastappended.kbuf, kbuf, ksiz);
  tbl->lastappended.ksiz = ksiz;
  return 0;
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
  if (memcmp(buf, FTBLMAGICDATA, strlen(FTBLMAGICDATA)) != 0) {
    ssftblsetecode(tbl, SSEMETA);
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
    if (sswrite(tbl->dfd, &e->ksiz,   sizeof(e->ksiz))   != 0) return -1;
    if (sswrite(tbl->dfd, e->kbuf,    e->ksiz)           != 0) return -1;
    if (sswrite(tbl->dfd, &e->doff,   sizeof(e->doff))   != 0) return -1;
    if (sswrite(tbl->dfd, &e->blksiz, sizeof(e->blksiz)) != 0) return -1;
  }
  return 0;
}

static int ssftbldumpblk(SSFTBL *tbl, int fd, char *buf, int bufsiz, int *sp) {
  if (sswrite(fd, buf, bufsiz) != 0) {
    ssftblsetecode(tbl, SSEWRITE);
    return -1;
  }
  *sp = bufsiz;
  return 0;
}

static char *ssftblloadblk(SSFTBL *tbl, int fd, uint64_t doff, int blksiz, int *sp) {
  char *buf;
  SSMALLOC(buf, blksiz);
  ssize_t nbytes = pread(fd, buf, blksiz, doff);
  if (nbytes != blksiz) {
    ssftblsetecode(tbl, SSEREAD);
    return NULL;
  }
  *sp = blksiz;
  return buf;
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
    if (ssread(tbl->dfd, &e->ksiz,   sizeof(e->ksiz))   != 0) return -1;
    SSMALLOC(e->kbuf, e->ksiz);
    if (ssread(tbl->dfd, e->kbuf,    e->ksiz)           != 0) return -1;
    if (ssread(tbl->dfd, &e->doff,   sizeof(e->doff))   != 0) return -1;
    if (ssread(tbl->dfd, &e->blksiz, sizeof(e->blksiz)) != 0) return -1;
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
  int bufsiz = 0;
  char *buf = tcmdbget(tbl->blkc, &e->doff, sizeof(e->doff), &bufsiz);
  if (buf == NULL) {
    buf = ssftblloadblk(tbl, tbl->dfd, e->doff, e->blksiz, &bufsiz); /* block cache miss */
    if (buf == NULL) return NULL;
    tcmdbput3(tbl->blkc, &e->doff, sizeof(e->doff), buf, bufsiz);
    if (tcmdbrnum(tbl->blkc) >= tbl->blkcnum)
      tcmdbcutfront(tbl->blkc, FTBLBLKCOUT);
  }
  if (buf == NULL || bufsiz <= 0)
    return NULL;
  int curpos = 0;
  while (curpos < bufsiz) {
    int ksiz = *((int*)(buf + curpos));
    curpos += sizeof(ksiz);
    char *kbuf = buf + curpos;
    curpos += ksiz;
    int vsiz = *((int*)(buf + curpos));
    curpos += sizeof(vsiz);
    char *vbuf = buf + curpos;
    curpos += vsiz;
    if (FTKEYCMPEQUAL(kbuf, ksiz, kb, ks)) {
      if (sp) *sp = vsiz;
      char *ret = NULL;
      SSMALLOC(ret, vsiz);
      memcpy(ret, vbuf, vsiz);
      return ret;
    }
  }
  SSFREE(buf);
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
