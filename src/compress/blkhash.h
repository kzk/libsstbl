#ifndef BLOCKHASH_H_
#define BLOCKHASH_H_

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

BLKHASH *blkhashnew(const char *ptr, size_t ptrsiz);
void blkhashdel(BLKHASH *bhash);

void blkhashaddhash(BLKHASH *bhash, int index, uint32_t hash);


#endif /* Not def: BLOCKHASH_H_ */
