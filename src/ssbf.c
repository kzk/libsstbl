#include <ssutil.h>
#include <ssbf.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* private function prototypes */
static void ssbfclear(SSBF *bf);
static void ssbfsetecode(SSBF *bf, int ecode);

/*-----------------------------------------------------------------------------
 * APIs
 */
SSBF *ssbfnew(uint64_t bsiz) {
  SSBF *bf = NULL;
  SSMALLOC(bf, sizeof(SSBF));
  ssbfclear(bf);
  bf->bsiz = bsiz;
  if (pthread_rwlock_init(&bf->mtx, NULL) != 0)
    goto err;
  return bf;
err:
  if (bf) ssbfdel(bf);
  return NULL;
}

void ssbfdel(SSBF *bf) {
  assert(bf);
  pthread_rwlock_destroy(&bf->mtx);
}

int ssbfopen(SSBF *bf, const char *path, int omode) {
  if (bf->fd >= 0) {
    ssbfsetecode(bf, SSEINVALID);
    return -1;
  }
  int fd = -1;
  void *ptr = NULL;
  int oflag = O_RDWR;
  if (omode & SSBFOCREAT) oflag |= O_CREAT;
  SSSYS_NOINTR(fd, open(path, oflag, 0644));
  if (fd < 0) {
    int ecode = SSEOPEN;
    switch (errno) {
    case EACCES: ecode = SSENOPERM; break;
    case ENOENT: ecode = SSENOFILE; break;
    case ENOTDIR: ecode = SSENOFILE; break;
    }
    ssbfsetecode(bf, ecode);
    goto err;
  }
  int prot = 0;
  if (omode & SSBFOWRITER) prot |= PROT_WRITE;
  if (omode & SSBFOREADER) prot |= PROT_READ;
  SSSYS_NOINTR(ptr, mmap(NULL, bf->bsiz, prot, MAP_SHARED, fd, 0));
  if (ptr == NULL) {
    ssbfsetecode(bf, SSEMMAP);
    goto err;
  }
  if (madvise(ptr, bf->bsiz, MADV_RANDOM) < 0) {
    ssbfsetecode(bf, SSEMMAP);
    goto err;
  }
  bf->fd = fd;
  bf->b = ptr;
  bf->omode = omode;
  return 0;
err:
  if (ptr) munmap(ptr, bf->bsiz);
  if (fd >= 0) close(fd);
  return -1;
}

int ssbfclose(SSBF *bf) {
  assert(bf);
  if (bf->b) {
    assert(bf->bsiz > 0);
    munmap(bf->b, bf->bsiz);
    bf->b = NULL;
  }
  if (bf->fd >= 0) {
    close(bf->fd);
    bf->fd = -1;
  }
  return 0;
}

#define SET_BIT(b, n) (b[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GET_BIT(b, n) (b[n/CHAR_BIT] &  (1<<(n%CHAR_BIT)))
int ssbfadd(SSBF *bf, const void *buf, int siz) {
  assert(bf && buf && siz > 0);
  if (!pthread_rwlock_wrlock(&bf->mtx)) {
    ssbfsetecode(bf, SSETHREAD);
    return -1;
  }
  int i;
  for (i = 0; i < bf->nfuncs; i++) {
    uint64_t v = bf->funcs[i]((const char*)buf, siz);
    uint64_t n = v % (bf->bsiz * CHAR_BIT);
    SET_BIT(bf->b, n);
  }
  pthread_rwlock_unlock(&bf->mtx);
  return 0;
}

int ssbfhas(SSBF *bf, const void *buf, int siz) {
  assert(bf && buf && siz > 0);
  if (!pthread_rwlock_rdlock(&bf->mtx)) {
    ssbfsetecode(bf, SSETHREAD);
    return -1;
  }
  int i;
  for (i = 0; i < bf->nfuncs; i++) {
    uint64_t v = bf->funcs[i]((const char*)buf, siz);
    uint64_t n = v % (bf->bsiz * CHAR_BIT);
    if (!GET_BIT(bf->b, n)) return 0;
  }
  pthread_rwlock_unlock(&bf->mtx);
  return 1;
}
#undef SET_BIT
#undef GET_BIT

/*-----------------------------------------------------------------------------
 * private functions
 */
static void ssbfclear(SSBF *bf) {
  assert(bf);
  bf->fd = -1;
  bf->b = NULL;
  bf->bsiz = 0;
  bf->omode = 0;
  bf->nfuncs = 0;
  bf->funcs = NULL;
  bf->ecode = SSESUCCESS;
}

static void ssbfsetecode(SSBF *bf, int ecode)
{
  assert(bf);
  bf->ecode = ecode;
}
