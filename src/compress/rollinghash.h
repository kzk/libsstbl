#ifndef ROLLINGHASH_H_
#define ROLLINGHASH_H_

typedef struct {
  size_t wsiz;
  uint32_t *rtbl; /* table used for fast update */
} ROLLINGHASH;

/* Create a rolling hash object.
 * The return value is the new rolling hash object.
 * `wsiz' specifies the windows size of the rolling hash.
 */
ROLLINGHASH *rollinghashnew(size_t wsiz);

/* Delete a rolling hash object.
 * `rhash' specifies the rolling hash object.
 */
void rollinghashdel(ROLLINGHASH *rhash);

/* Get the result of the hash of the window size.
 * `rhash' specifies the rolling hash object.
 * `ptr' specifies the pointer to the data.
 * The return value is the hash value of the values within window size.
 */
uint32_t rollinghashdohash(ROLLINGHASH *rhash, const char *ptr);

/* Rolling the hash value.
 * `rhash' specifies the rolling hash object.
 * `oldhash' specifies the old hash value (current window hash value).
 * `old_first_byte' specifies the first byte of the current window.
 * `new_last_byte' specifies the last byte of the next window.
 * The return value is the hash value of the following value:
 *   
 */
uint32_t rollinghashupdate(ROLLINGHASH *rhash,
                           uint32_t oldhash,
                           const char old_first_byte,
                           const char new_last_byte);

#endif /* Not def: ROLLINGHASH_H_ */
