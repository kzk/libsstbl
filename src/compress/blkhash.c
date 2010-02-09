#include <ssutil.h>
#include <compress/blkhash.h>

/* const or default parameters */
#define BLKHASHBLKSIZ 16
#define BLKHASHMAXPROBES 16

/* private function prototypes */
static uint32_t getnumblocks(BLKHASH *bhash);
static uint32_t gethashtblindex(BLKHASH *bhash, uint32_t hash);
static int calctblsize(size_t ptrsiz);
static int addblk(BLKHASH *bhash, uint32_t hash);
static int nextindextoadd(BLKHASH *bhash);
static int scanblks(BLKHASH *bhash, int hashtblidx, const char *ptr);
static int replaceifbettermatch(BLKHASHMATCH *m, int matchsize, int targetoff, int sourceoff);
static void setecode(BLKHASH *bhash, int ecode);

/*-----------------------------------------------------------------------------
 * APIs
 */
BLKHASH *blkhashnew(const char *ptr, size_t ptrsiz) {
  uint32_t i;
  uint32_t tblsiz = calctblsize(ptrsiz);
  if (tblsiz <= 0) return NULL;
  BLKHASH *bhash;
  SSMALLOC(bhash, sizeof(BLKHASH));
  bhash->ptr = ptr;
  bhash->ptrsiz = ptrsiz;
  bhash->blksiz = BLKHASHBLKSIZ;
  bhash->lastaddedblknum = -1;
  bhash->ecode = SSESUCCESS;
  SSMALLOC(bhash->hashtbl, tblsiz * sizeof(uint32_t));
  bhash->hashtblmask = ((uint32_t)tblsiz) - 1;
  SSMALLOC(bhash->nextblktbl, getnumblocks(bhash) * sizeof(int));
  SSMALLOC(bhash->lastblktbl, getnumblocks(bhash) * sizeof(int));
  for (i = 0; i < tblsiz; i++)
    bhash->hashtbl[i] = -1;
  for (i = 0; i < getnumblocks(bhash); i++) {
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
  match->size = 0;
  match->targetoff = -1;
  match->sourceoff = -1;
  int blknum = blkhashfindfirstmatch(bhash, hash, targetptr);
  for (; blknum >= 0; blknum = blkhashfindnextmatch(bhash, blknum, targetptr)) {
    int matchsize = bhash->blksiz;
    int sourcematchoff = blknum * bhash->blksiz;
    int sourcematchend = sourcematchoff + bhash->blksiz;
    int targetmatchoff = targetptr - targetptrbegin;
    int targetmatchend = targetmatchoff + bhash->blksiz;
    if (replaceifbettermatch(match, matchsize, targetmatchoff, sourcematchoff))
      ret = blknum;
  }
  return ret;
}

/*-----------------------------------------------------------------------------
 * private functions
 */
static uint32_t getnumblocks(BLKHASH *bhash) {
  assert(bhash);
  return (bhash->ptrsiz / bhash->blksiz);
}

static uint32_t gethashtblindex(BLKHASH *bhash, uint32_t hash) {
  assert(bhash);
  return hash & bhash->hashtblmask;
}

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

static int nextindextoadd(BLKHASH *bhash) {
  return (bhash->lastaddedblknum + 1) * bhash->blksiz;
}

static int scanblks(BLKHASH *bhash, int blknum, const char* targetptr) {
  int n = 0;
  while (blknum >= 0
         && memcmp(targetptr,
                   bhash->ptr + (bhash->blksiz * blknum),
                   bhash->blksiz) != 0) {
    if (++n > BLKHASHMAXPROBES)
      return -1; /* avoid too much scanning */
    assert(0 <= blknum && (unsigned int)blknum < getnumblocks(bhash));
    blknum = bhash->nextblktbl[blknum];
  }
  return blknum;
}

static int replaceifbettermatch(BLKHASHMATCH *m, int matchsize,
                                int targetoff, int sourceoff) {
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
