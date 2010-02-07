#ifndef BLOCKHASH_H_
#define BLOCKHASH_H_

typedef struct {
  size_t size;
  int targetoff;
  int sourceoff;
} BLKHASHMATCH;

typedef struct {
  const char *ptr;
  size_t ptrsiz;
  size_t blksiz;
  int lastaddedblknum;
  uint32_t *hashtbl;
  uint32_t hashtblmask;
  int *nextblktbl;
  int *lastblktbl;
  int ecode;
} BLKHASH;

/* Create a block hash object.
 * The return value is the new block hash object.
 * `ptr' specifies the pointer to the data.
 * `ptrsiz' specifies the size of the data
 */
BLKHASH *blkhashnew(const char *ptr, size_t ptrsiz);

/* Delete a block hash object.
 * `bhash' specifies the block hash object.
 */
void blkhashdel(BLKHASH *bhash);

/* Add a block to the block hash object.
 * `bhash' specifies the block hash object.
 * `index' specfies the offset of the data.
 * `hash' specifies the hash of the data starting from `index'.
 */
void blkhashaddhash(BLKHASH *bhash, int index, uint32_t hash);

int blkhashfindbestmatch(BLKHASH *bhash, uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz,
                         BLKHASHMATCH *match);
int blkhashfindfirstmatch(BLKHASH *bhash, uint32_t hash, const char *ptr);
int blkhashfindnextmatch(BLKHASH *bhash, int blknum, const char *ptr);

#endif /* Not def: BLOCKHASH_H_ */
