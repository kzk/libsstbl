#include <ssutil.h>
#include <compress/vcdiff.h>
#include <compress/blkhash.h>
#include <compress/rollinghash.h>

/* private function prototypes */
static int findbestmatch(uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz,
                         BLKHASH *bhash);

/*-----------------------------------------------------------------------------
 * APIs
 */
char *vcdiffencode(const char *ptr, size_t ptrsiz) {
  if (ptr == NULL || ptrsiz == 0) return NULL;
  BLKHASH *bhash = NULL;
  ROLLINGHASH *rhash = NULL;
  bhash = blkhashnew(ptr, ptrsiz);
  if (bhash == NULL) return NULL;
  if (ptrsiz < bhash->blksiz) {
    /* too small to compress */
    char *ret;
    SSMALLOC(ret, ptrsiz);
    memcpy(ret, ptr, ptrsiz);
    return ret;
  }
  size_t blksiz = bhash->blksiz;
  const char *unencodedptr = ptr;
  const char *ptrbegin = ptr;
  const char *ptrend = ptr + ptrsiz;
  const char *ptrlastblk = ptrend - blksiz;
  rhash = rollinghashnew(blksiz);
  if (rhash == NULL) goto error;
  uint32_t hash = rollinghashdohash(rhash, ptr);
  while (1) {
    int encoded = findbestmatch(hash, ptr, unencodedptr, ptrend - unencodedptr, bhash);
    if (encoded > 0) {
      /* matching block was find */
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
  blkhashdel(bhash);
  rollinghashdel(rhash);
  return NULL;
error:
  if (bhash) blkhashdel(bhash);
  if (rhash) rollinghashdel(rhash);
  return NULL;
}

/*-----------------------------------------------------------------------------
 * private functions
 */
static int findbestmatch(uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz, BLKHASH *bhash) {
  int blknum;
  BLKHASHMATCH match;
  blknum = blkhashfindbestmatch(bhash, hash, targetptr, targetptrbegin, targetsiz, &match);
  if (blknum == -1) return -1; /* match not found */
  return match.targetoff + match.size;
}
