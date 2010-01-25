#include <ssutil.h>
#include <ssftbl.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define SSFTBLBLOCKSIZ (64 * 1024)

/* private function prototypes */
static void ssftblclear(SSFTBL *tbl);
static int ssftblwriteopenimpl(SSFTBL *tbl, const char *path);
static int ssftblreadopenimpl(SSFTBL *tbl, const char *path);
static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz);
static int ssftbldumpmeta(SSFTBL *tbl);
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
  if (tbl->lastappendedkey)
    SSFREE(tbl->lastappendedkey);
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
  switch (omode) {
  case SSFTBLOWRITER:
    return ssftblwriteopenimpl(tbl, path);
  case SSFTBLOREADER:
    return ssftblreadopenimpl(tbl, path);
  default:
    ;
  }
  ssftblsetecode(tbl, SSEINVALID);
  return -1;
}

int ssftblappend(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  assert(tbl && kbuf && ksiz > 0 && vbuf && vsiz > 0);
  if (tbl->omode != SSFTBLOWRITER) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  if (tbl->lastappendedkey != NULL) {
    if (FTKEYCMPGREATER(tbl->lastappendedkey, tbl->lastappendedkeysiz, kbuf, ksiz)) {
      /* keys must be appended in the lexicographically ascending order */
      ssftblsetecode(tbl, SSEINVALID);
      return -1;
    }
  }
  return ssftblappendimpl(tbl, kbuf, ksiz, vbuf, vsiz);
}

int ssftblclose(SSFTBL *tbl) {
  int r;
  if (tbl->dfd < 0) {
    ssftblsetecode(tbl, SSEINVALID);
    return -1;
  }
  if (tbl->dfd >= 0) {
    SSSYS_NOINTR(r, fsync(tbl->dfd));
    SSSYS_NOINTR(r, close(tbl->dfd));
    tbl->dfd = -1;
  }
  if (tbl->ifd >= 0) {
    SSSYS_NOINTR(r, fsync(tbl->dfd));
    SSSYS_NOINTR(r, close(tbl->ifd));
    tbl->ifd = -1;
  }
  tbl->curblksiz = 0;
  if (tbl->lastappendedkey) {
    SSFREE(tbl->lastappendedkey);
    tbl->lastappendedkey = NULL;
    tbl->lastappendedkeysiz = 0;
  }
  return 0;
}

/*-----------------------------------------------------------------------------
 * private functions
 */
static void ssftblclear(SSFTBL *tbl) {
  assert(tbl);
  tbl->dfd = -1;
  tbl->ifd = -1;
  tbl->blksiz = SSFTBLBLOCKSIZ;
  tbl->rnum = 0;
  tbl->curblksiz = 0;
  tbl->lastappendedkey = NULL;
  tbl->lastappendedkeysiz = 0;
  tbl->omode = 0;
  tbl->ecode = SSESUCCESS;
}

static int ssftblwriteopenimpl(SSFTBL *tbl, const char *path) {
  int r, i;
  int fds[2]; /* fd[0] -> dfd, fd[1] -> ifd */
  int oflag = O_WRONLY | O_CREAT | O_TRUNC | O_APPEND;
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
  tbl->omode = SSFTBLOWRITER;
  return 0;
err:
  for (i = 0; i < 2; i++)
    if (fds[i] >= 0)
      SSSYS_NOINTR(r, close(fds[i]));
  return -1;
}

static int ssftblreadopenimpl(SSFTBL *tbl, const char *path) {
  return 0;
}

static int ssftblappendimpl(SSFTBL *tbl, const void *kbuf, int ksiz, const void *vbuf, int vsiz) {
  uint64_t doff = lseek(tbl->dfd, 0, SEEK_END);
  if (sswrite(tbl->dfd, &ksiz, sizeof(ksiz)) != 0) goto err;
  if (sswrite(tbl->dfd, kbuf, ksiz) != 0) goto err;
  if (sswrite(tbl->dfd, &vsiz, sizeof(vsiz)) != 0) goto err;
  if (sswrite(tbl->dfd, vbuf, vsiz) != 0) goto err;
  tbl->curblksiz += vsiz;
  assert(tbl->blksiz > 0);
  if (tbl->lastappendedkey == NULL || tbl->curblksiz >= tbl->blksiz) {
    /* record into index file */
    if (sswrite(tbl->ifd, &ksiz, sizeof(ksiz)) != 0) goto err;
    if (sswrite(tbl->ifd, kbuf, ksiz) != 0) goto err;
    if (sswrite(tbl->ifd, &doff, sizeof(doff)) != 0) goto err;
    /* move to the next block */
    tbl->curblksiz = 0;
  }
  SSREALLOC(tbl->lastappendedkey, tbl->lastappendedkey, tbl->lastappendedkeysiz);
  memcpy(tbl->lastappendedkey, kbuf, ksiz);
  tbl->lastappendedkeysiz = ksiz;
  return 0;
err:
  ssftblsetecode(tbl, SSEWRITE);
  return -1;
}

static int ssftbldumpmeta(SSFTBL *tbl) {
  assert(tbl);
  return 0;
}

static int ssftblkeycmp(const char *s1, size_t n1, const char *s2, size_t n2) {
  assert(s1 && s2);
  if(n1 < n2) return -1;
  else if(n1 > n2) return 1;
  return memcmp(s1, s2, n1);
}

static void ssftblsetecode(SSFTBL *tbl, int ecode) {
  assert(tbl);
  tbl->ecode = ecode;
}
