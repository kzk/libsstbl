#include <ssutil.h>
#include <ssbf.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* private function prototypes */
static void ssbfclear(SSBF *bf);
static int ssbfopenimpl(SSBF *bf, const char *path, int omode);
static void ssbfsetecode(SSBF *bf, int ecode);

/*-----------------------------------------------------------------------------
 * APIs
 */
SSBF *ssbfnew(uint64_t bsiz) {
  SSBF *bf = NULL;
  SSMALLOC(bf, sizeof(SSBF));
  ssbfclear(bf);
  bf->mapsiz = bsiz;
  if (pthread_rwlock_init(&bf->mtx, NULL) != 0)
    goto err;
  return bf;
err:
  if (bf) ssbfdel(bf);
  return NULL;
}

void ssbfdel(SSBF *bf) {
  assert(bf);
  if (bf->fd >= 0)
    ssbfclose(bf);
  pthread_rwlock_destroy(&bf->mtx);
  SSFREE(bf);
}

int ssbfopen(SSBF *bf, const char *path, int omode) {
  if (bf->fd >= 0) {
    ssbfsetecode(bf, SSEINVALID);
    return -1;
  }
  return ssbfopenimpl(bf, path, omode);
}

int ssbfclose(SSBF *bf) {
  assert(bf);
  if (bf->map) {
    assert(bf->mapsiz > 0);
    munmap(bf->map, bf->mapsiz);
    bf->map = NULL;
    bf->mapsiz = 0;
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
  if (pthread_rwlock_wrlock(&bf->mtx)) {
    ssbfsetecode(bf, SSETHREAD);
    return -1;
  }
  unsigned int i;
  for (i = 0; i < bf->nfuncs; i++) {
    uint64_t v = bf->funcs[i]((const char*)buf, siz);
    uint64_t n = v % (bf->mapsiz * CHAR_BIT);
    SET_BIT(bf->map, n);
  }
  pthread_rwlock_unlock(&bf->mtx);
  return 0;
}

int ssbfhas(SSBF *bf, const void *buf, int siz) {
  assert(bf && buf && siz > 0);
  if (pthread_rwlock_rdlock(&bf->mtx)) {
    ssbfsetecode(bf, SSETHREAD);
    return -1;
  }
  unsigned int i;
  for (i = 0; i < bf->nfuncs; i++) {
    uint64_t v = bf->funcs[i]((const char*)buf, siz);
    uint64_t n = v % (bf->mapsiz * CHAR_BIT);
    if (!GET_BIT(bf->map, n)) return 0;
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
  bf->map = NULL;
  bf->mapsiz = 0;
  bf->omode = 0;
  bf->nfuncs = 0;
  bf->funcs = NULL;
  bf->ecode = SSESUCCESS;
}

static int ssbfopenimpl(SSBF *bf, const char *path, int omode) {
  
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
  ptr = mmap(NULL, bf->mapsiz, prot, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    ssbfsetecode(bf, SSEMMAP);
    goto err;
  }
  if (madvise(ptr, bf->mapsiz, MADV_RANDOM) < 0) {
    ssbfsetecode(bf, SSEMMAP);
    goto err;
  }
  bf->fd = fd;
  bf->map = ptr;
  bf->omode = omode;
  return 0;
err:
  if (ptr) munmap(ptr, bf->mapsiz);
  if (fd >= 0) close(fd);
  return -1;
}

static void ssbfsetecode(SSBF *bf, int ecode)
{
  assert(bf);
  bf->ecode = ecode;
}
