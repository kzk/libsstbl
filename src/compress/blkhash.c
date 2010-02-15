#include <ssutil.h>
#include <compress/blkhash.h>

/* const or default parameters */
#define BLKHASHBLKSIZ 16
#define BLKHASHMAXPROBES 16

/* private function prototypes */
static int getnumblocks(BLKHASH *bhash);
static uint32_t gethashtblindex(BLKHASH *bhash, uint32_t hash);
static int calctblsize(size_t ptrsiz);
static int addblk(BLKHASH *bhash, uint32_t hash);
static int nextindextoadd(BLKHASH *bhash);
static int scanblks(BLKHASH *bhash, int hashtblidx, const char *ptr);
static int matchleft(const char *ptr1, const char *ptr2, int maxsiz);
static int matchright(const char *ptr1, const char *ptr2, int maxsiz);
static int replaceifbettermatch(BLKHASHMATCH *m, int matchsize, int targetoff, int sourceoff);
static void setecode(BLKHASH *bhash, int ecode);

/*-----------------------------------------------------------------------------
 * APIs
 */
BLKHASH *blkhashnew(const char *sourceptr, size_t sourceptrsiz) {
  uint32_t i;
  uint32_t tblsiz = calctblsize(sourceptrsiz);
  if (tblsiz <= 0) return NULL;
  BLKHASH *bhash;
  SSMALLOC(bhash, sizeof(BLKHASH));
  bhash->ptr = sourceptr;
  bhash->ptrsiz = sourceptrsiz;
  bhash->blksiz = BLKHASHBLKSIZ;
  bhash->lastaddedblknum = -1;
  bhash->ecode = SSESUCCESS;
  SSMALLOC(bhash->hashtbl, tblsiz * sizeof(uint32_t));
  bhash->hashtblmask = ((uint32_t)tblsiz) - 1;
  SSMALLOC(bhash->nextblktbl, getnumblocks(bhash) * sizeof(int));
  SSMALLOC(bhash->lastblktbl, getnumblocks(bhash) * sizeof(int));
  for (i = 0; i < tblsiz; i++)
    bhash->hashtbl[i] = -1;
  for (i = 0; i < (uint32_t)getnumblocks(bhash); i++) {
    bhash->nextblktbl[i] = -1;
    bhash->lastblktbl[i] = -1;
  }
  return bhash;
}

void blkhashdel(BLKHASH *bhash) {
  assert(bhash);
  SSFREE(bhash->hashtbl);
  SSFREE(bhash->nextblktbl);
  SSFREE(bhash->lastblktbl);
  SSFREE(bhash);
}

int blkhashaddhash(BLKHASH *bhash, int index, uint32_t hash) {
  if (index != nextindextoadd(bhash)) return 0;
  return addblk(bhash, hash);
}

int blkhashfindfirstmatch(BLKHASH *bhash, uint32_t hash, const char *targetptr) {
  int hashtblidx = gethashtblindex(bhash, hash);
  int blknum = bhash->hashtbl[hashtblidx];
  return scanblks(bhash, blknum, targetptr);
}

int blkhashfindnextmatch(BLKHASH *bhash, int blknum, const char *targetptr) {
  if (blknum < 0 || blknum >= getnumblocks(bhash)) {
    setecode(bhash, SSEINVALID);
    return -1;
  }
  return scanblks(bhash, bhash->nextblktbl[blknum], targetptr);
}

int blkhashfindbestmatch(BLKHASH *bhash, uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz,
                         BLKHASHMATCH *match) {
  int ret = -1;
  int matchingcnt = 0;
  int maxmatchingblknum = (bhash->blksiz >= 32) ? 32 : (32 * (32 / bhash->blksiz));
  match->size = 0;
  match->targetoff = -1;
  match->sourceoff = -1;
  int blknum = blkhashfindfirstmatch(bhash, hash, targetptr);
  for (; blknum >= 0; blknum = blkhashfindnextmatch(bhash, blknum, targetptr)) {
    assert(0 <= blknum && blknum < getnumblocks(bhash));
    matchingcnt++;
    if (matchingcnt > maxmatchingblknum) break;
    int matchsiz = bhash->blksiz;
    int sourcematchoff = blknum * bhash->blksiz;
    int sourcematchend = sourcematchoff + bhash->blksiz;
    int targetmatchoff = targetptr - targetptrbegin;
    int targetmatchend = targetmatchoff + bhash->blksiz;
    /* extend the match towards the beginning of data */
    int sourcemaxsizleft = sourcematchoff;
    int targetmaxsizleft = targetmatchoff;
    int maxsizleft = SSMIN(sourcemaxsizleft, targetmaxsizleft);
    int matchsizleft = matchleft(bhash->ptr + sourcematchoff, targetptr, maxsizleft);
    /* extend the match towards the end of data */
    int sourcemaxsizright = bhash->ptrsiz - sourcematchend;
    int targetmaxsizright = targetsiz - targetmatchend;
    int maxsizright = SSMIN(sourcemaxsizright, targetmaxsizright);
    int matchsizright = matchright(bhash->ptr + sourcematchend,
                                   targetptrbegin + targetmatchend,
                                   maxsizright);
    /* update matchsiz */
    matchsiz += matchsizleft + matchsizright;
    /* replace the current match if longer one is found */
    if (replaceifbettermatch(match, matchsiz, targetmatchoff, sourcematchoff))
      ret = blknum;
  }
  return ret;
}

/*-----------------------------------------------------------------------------
 * private functions
 */

/* Get the number of blocks in the source data.
   `bhash' specifies the block hash object.
   The return value is the number of blocks in the source data.
 */
static int getnumblocks(BLKHASH *bhash) {
  assert(bhash);
  return (bhash->ptrsiz / bhash->blksiz);
}

/* Get the index of the hashtbl.
   `bhash' specifies the block hash object.
   The return value is the index of bhash->hashtbl.
 */
static uint32_t gethashtblindex(BLKHASH *bhash, uint32_t hash) {
  assert(bhash);
  return hash & bhash->hashtblmask;
}

/* Calc the number of entries of hashtbl.
   `ptrsiz' specifies the size of the source data.
   The return value is the number of entries of bhash->hashtbl.
   Increasing this value is a tradeoff between the performance and the memory
   consumption.
 */
static int calctblsize(size_t ptrsiz) {
  int minsiz = (ptrsiz / sizeof(int)) + 1;
  int tblsiz = 1;
  while (tblsiz < minsiz) {
    tblsiz <<= 1;
    assert(tblsiz > 0);
  }
  assert((tblsiz & (tblsiz - 1)) == 0);
  assert(0 < tblsiz && tblsiz < minsiz * 2);
  return tblsiz;
}

/* Add the block to the block hash object.
   `bhash' specifies the block hash object.
   `hash' specifies the hash value of the adding block.
   The return value is 0 if success, otherwise -1.
 */
static int addblk(BLKHASH *bhash, uint32_t hash) {
  int blknum = bhash->lastaddedblknum + 1;
  int totalblknum = (bhash->ptrsiz / bhash->blksiz) + 1;
  if (blknum >= totalblknum) {
    setecode(bhash, SSEINVALID);
    return -1;
  }
  if (bhash->nextblktbl[blknum] != -1) {
    setecode(bhash, SSEINVALID);
    return -1;
  }
  uint32_t hashtblindex = gethashtblindex(bhash, hash);
  int firstmatchingblk = bhash->hashtbl[hashtblindex];
  if (firstmatchingblk < 0) {
    /* first entry with this hash value */
    bhash->hashtbl[hashtblindex] = blknum;
    bhash->lastblktbl[blknum] = blknum;
  } else {
    /* the blocks which has this hash value already appeared several times */
    int lastmatchingblk = bhash->lastblktbl[firstmatchingblk];
    if (bhash->nextblktbl[lastmatchingblk] != -1) {
      setecode(bhash, SSEINVALID);
      return -1;
    }
    bhash->nextblktbl[lastmatchingblk] = blknum;
    bhash->lastblktbl[firstmatchingblk] = blknum;
  }
  bhash->lastaddedblknum = blknum;
  return 0;
}

/* Get the index of the next block to add in the source data.
   `bhash' specifies the block hash object.
   The return value is the index in source data.
 */
static int nextindextoadd(BLKHASH *bhash) {
  return (bhash->lastaddedblknum + 1) * bhash->blksiz;
}

/* Scan the blocks to find the matching block.
   `bhash' specifies the block hash object.
   `blknum' specifies the block number.
   `targetptr' specifies the pointer to the data to be matched.
   The return value is the matched block number.
 */
static int scanblks(BLKHASH *bhash, int blknum, const char* targetptr) {
  int n = 0;
  while (blknum >= 0
         && memcmp(targetptr,
                   bhash->ptr + (bhash->blksiz * blknum),
                   bhash->blksiz) != 0) {
    if (++n > BLKHASHMAXPROBES)
      return -1; /* avoid too much scanning */
    assert(0 <= blknum && blknum < getnumblocks(bhash));
    blknum = bhash->nextblktbl[blknum];
  }
  return blknum;
}

/* Count the matching bytes to the left between two pointers.
   `ptr1' specifies the data pointer.
   `ptr2' specifies the another data pointer.
   `maxsiz' specifies the max size of the matching region.
   The return value is the number of matching bytes.
 */
static int matchleft(const char *ptr1, const char *ptr2, int maxsiz) {
  int matchsiz = 0;
  while (matchsiz < maxsiz) {
    --ptr1;
    --ptr2;
    if (*ptr1 != *ptr2) break;
    matchsiz++;
  }
  return matchsiz;
}

/* Count the matching bytes to the right between two pointers.
   `ptr1' specifies the data pointer.
   `ptr2' specifies the another data pointer.
   `maxsiz' specifies the max size of the matching region.
   The return value is the number of matching bytes.
 */
static int matchright(const char *ptr1, const char *ptr2, int maxsiz) {
  int matchsiz = 0;
  while (matchsiz < maxsiz) {
    if (*ptr1 != *ptr2) break;
    ++ptr1;
    ++ptr2;
    ++matchsiz;
  }
  return matchsiz;
}

/* Replace the BLKHASHMATCH if longer one is found.
   `m' specifies the pointer to the match result structure.
   `matchsize' specifies the newly found matching size.
   `targetoff' specifies the target offset of the newly found matching region.
   `sourceoff' specifies the source offset of the newly found matching region.
   The return value is 1 if replaced, otherwise 0.
 */
static int replaceifbettermatch(BLKHASHMATCH *m, int matchsize,
                                int targetoff, int sourceoff) {
  assert(m != NULL);
  if (m->size < (unsigned int)matchsize) {
    m->size = matchsize;
    m->targetoff = targetoff;
    m->sourceoff = sourceoff;
    return 1;
  }
  return 0;
}

static void setecode(BLKHASH *bhash, int ecode) {
  assert(bhash);
  bhash->ecode = ecode;
}
