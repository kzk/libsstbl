#ifndef BLOCKHASH_H_
#define BLOCKHASH_H_

#if defined(__cplusplus)
#define BLKHASH_CLINKAGEBEGIN extern "C" {
#define BLKHASH_CLINKAGEEND }
#else
#define BLKHASH_CLINKAGEBEGIN
#define BLKHASH_CLINKAGEEND
#endif
BLKHASH_CLINKAGEBEGIN

#include <stdio.h>
#include <stdint.h>

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
   `sourceptr' specifies the pointer to the dictionary data.
   `sourceptrsiz' specifies the size of the dictionary data.
   The return value is the new block hash object.
*/
BLKHASH *blkhashnew(const char *sourceptr, size_t sourceptrsiz);

/* Delete a block hash object.
   `bhash' specifies the block hash object.
 */
void blkhashdel(BLKHASH *bhash);

/* Add a block to the block hash object.
   `bhash' specifies the block hash object.
   `index' specfies the offset of the data.
   `hash' specifies the hash of the data starting from `index'.
   The return value is zero if success, otherwise -1.
 */
int blkhashaddhash(BLKHASH *bhash, int index, uint32_t hash);

int blkhashfindbestmatch(BLKHASH *bhash, uint32_t hash, const char *targetptr,
                         const char *targetptrbegin, size_t targetsiz,
                         BLKHASHMATCH *match);

/* Find the first block which has the same data pointed by targetptr.
   `bhash' specifies the block hash object.
   `hash' specifies the hash value pointed by ptr. This value is used for fast
   comparison of the memory region.
   `targetptr' specifies the pointer to the data to find.
   The return value is block index in the source data if success, otherwise -1.
 */
int blkhashfindfirstmatch(BLKHASH *bhash, uint32_t hash, const char *targetptr);

/* Find the next block which has the same data pointed by ptr.
   `bhash' specifies the block hash object.
   `blknum' specifies the previous block number which the data appeared previously.
   This value should be the return value of blkhashfindfirstmatch().
   `targetptr' specifies the pointer to the data to find.
   The return value is block index in the source data if success, otherwise -1.
 */
int blkhashfindnextmatch(BLKHASH *bhash, int blknum, const char *targetptr);

BLKHASH_CLINKAGEEND
#endif
