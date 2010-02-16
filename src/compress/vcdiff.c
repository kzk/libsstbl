#include <ssutil.h>
#include <compress/vcdiff.h>
#include <compress/blkhash.h>
#include <compress/rollinghash.h>

enum VCDIFFOP {
  OP_ADD,
  OP_COPY
};

typedef struct {
  enum VCDIFFOP *ops;
  /* for add operation */
  int addopnum;
  const char **adddata;
  size_t *adddatasizs;
  /* for copy operation */
  int copyopnum;
  int *copyoffsets;
  size_t *copysizs;
} VCDIFFENC;

/* private function prototypes */
static VCDIFFENC *encodernew(void);
static void encoderdel(VCDIFFENC *enc);
static char *encoderout(VCDIFFENC *enc);
static int adddata(VCDIFFENC *enc, const char *data, size_t size);
static int copydata(VCDIFFENC *enc, int offset, size_t size);
static int findbestmatch(VCDIFFENC *enc, uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz,
                         BLKHASH *bhash);

/*-----------------------------------------------------------------------------
 * APIs
 */
char *vcdiffencode(const char *ptr, size_t ptrsiz) {
  if (ptr == NULL || ptrsiz == 0) return NULL;
  char *compressedptr = NULL;
  VCDIFFENC *enc = NULL;
  BLKHASH *bhash = NULL;
  ROLLINGHASH *rhash = NULL;
  enc = encodernew();
  if (enc == NULL) goto error;
  bhash = blkhashnew(ptr, ptrsiz);
  if (bhash == NULL) goto error;
  if (ptrsiz < bhash->blksiz) {
    /* too small to compress */
    adddata(enc, ptr, ptrsiz);
    goto end;
  }
  size_t blksiz = bhash->blksiz;
  const char *unencodedptr = ptr;
  const char *ptrbegin = ptr;
  const char *ptrend = ptr + ptrsiz;
  const char *ptrlastblk = ptrend - blksiz;
  rhash = rollinghashnew(blksiz);
  if (rhash == NULL) goto error;
  /* iterate througth the data */
  uint32_t hash = rollinghashdohash(rhash, ptr);
  while (1) {
    assert(unencodedptr <= ptrend);
    int encoded = findbestmatch(enc, hash, ptr, unencodedptr, ptrend - unencodedptr, bhash);
    if (encoded > 0) {
      /* matching block is found */
      unencodedptr += encoded;
      if (ptr > ptrlastblk) break;
      while (ptr < unencodedptr) {
        blkhashaddhash(bhash, ptr - ptrbegin, hash);
        hash = rollinghashupdate(rhash, hash, ptr[0], ptr[blksiz]);
        ptr++;
      }
      assert(ptr == unencodedptr);
    } else {
      /* no matching block was found */
      if (ptr + 1 > ptrlastblk) break;
      blkhashaddhash(bhash, ptr - ptrbegin, hash);
      hash = rollinghashupdate(rhash, hash, ptr[0], ptr[blksiz]);
      ptr++;
    }
  }
  /* add remaining unencoded data */
  if (unencodedptr < ptrend)
    adddata(enc, unencodedptr, ptrend - unencodedptr);
end:
  compressedptr = encoderout(enc);
  blkhashdel(bhash);
  rollinghashdel(rhash);
  encoderdel(enc);
  return compressedptr;
error:
  if (bhash) blkhashdel(bhash);
  if (rhash) rollinghashdel(rhash);
  if (enc) encoderdel(enc);
  return NULL;
}

/*-----------------------------------------------------------------------------
 * private functions
 */
static VCDIFFENC *encodernew(void) {
  VCDIFFENC *e;
  SSMALLOC(e, sizeof(VCDIFFENC));
  e->ops = NULL;
  e->addopnum = 0;
  e->adddata = NULL;
  e->adddatasizs = NULL;
  e->copyopnum = 0;
  e->copyoffsets = NULL;
  e->copysizs = NULL;
  return e;
}

static void encoderdel(VCDIFFENC *e) {
  if (e->ops) {
    free(e->ops);
    e->ops = NULL;
  }
  if (e->adddata) {
    free(e->adddata);
    e->adddata = NULL;
  }
  if (e->adddatasizs) {
    free(e->adddatasizs);
    e->adddatasizs = NULL;
  }
  if (e->copyoffsets) {
    free(e->copyoffsets);
    e->copyoffsets = NULL;
  }
  if (e->copysizs) {
    free(e->copysizs);
    e->copysizs = NULL;
  }
}

static char *encoderout(VCDIFFENC *enc) {
  return NULL;
}

static int encodeop(VCDIFFENC *e, enum VCDIFFOP op) {
  int nops = e->addopnum + e->copyopnum + 1;
  SSREALLOC(e->ops, e->ops, sizeof(e->ops[0]) * nops);
  e->ops[nops-1] = op;
  return 0;
}

static int adddata(VCDIFFENC *e, const char *data, size_t size) {
  encodeop(e, OP_ADD);
  e->addopnum++;
  int nops = e->addopnum + e->copyopnum;
  assert(nops == e->addopnum + e->copyopnum);
  SSREALLOC(e->adddata, e->adddata, sizeof(e->adddata[0]) * nops);
  SSREALLOC(e->adddatasizs, e->adddatasizs, sizeof(e->adddata[0]) * nops);
  e->adddata[nops-1] = data;
  e->adddatasizs[nops-1] = size;
  return 0;
}

static int copydata(VCDIFFENC *e, int offset, size_t size) {
  encodeop(e, OP_COPY);
  e->copyopnum++;
  int nops = e->addopnum + e->copyopnum;
  SSREALLOC(e->copyoffsets, e->copyoffsets, sizeof(e->copyoffsets[0]) * nops);
  SSREALLOC(e->copysizs, e->copysizs, sizeof(e->copysizs[0]) * nops);
  e->copyoffsets[nops-1] = offset;
  e->copysizs[nops-1] = size;
  return 0;
}

static int findbestmatch(VCDIFFENC *enc, uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz, BLKHASH *bhash) {
  int blknum;
  BLKHASHMATCH match;
  blknum = blkhashfindbestmatch(bhash, hash, targetptr, targetptrbegin, targetsiz, &match);
  if (blknum == -1) return -1; /* match not found */
  if (match.targetoff > 0) {
    /* ADD */
    adddata(enc, targetptrbegin, match.targetoff);
  }
  /* COPY */
  copydata(enc, match.sourceoff, match.size);
  return match.targetoff + match.size;
}
