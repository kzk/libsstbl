#include <ssutil.h>
#include <compress/blkhash.h>

/* const or default parameters */
#define BLKHASHBLKSIZ 16

/* private function prototypes */
static uint32_t getnumblocks(BLKHASH *bhash);
static uint32_t gethashtblindex(BLKHASH *bhash, uint32_t hash);
static int calctblsize(size_t ptrsiz);
static int addblk(BLKHASH *bhash, uint32_t hash);
static int nextindextoadd(BLKHASH *bhash); 
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
  if (bhash == NULL) return;
  SSFREE(bhash->hashtbl);
  SSFREE(bhash->nextblktbl);
  SSFREE(bhash->lastblktbl);
  SSFREE(bhash);
}

void blkhashaddhash(BLKHASH *bhash, int index, uint32_t hash) {
  if (index != nextindextoadd(bhash)) return;
  addblk(bhash, hash);
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

static void setecode(BLKHASH *bhash, int ecode) {
  assert(bhash);
  bhash->ecode = ecode;
}
