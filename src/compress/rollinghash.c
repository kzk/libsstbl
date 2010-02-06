#include <ssutil.h>
#include <compress/rollinghash.h>

#define RHASHMULT 257
#define RHASHBASE (1 << 23)

/* private function prototypes */
uint32_t *initremovetbl(size_t wsiz);

/* private macros */
#define MODBASE(operand)    (((uint32_t)operand) & (RHASHBASE - 1))
#define MODBASEINV(operand) (MODBASE(((uint32_t)(0)) - ((uint32_t)operand)))

/*-----------------------------------------------------------------------------
 * APIs
 */
ROLLINGHASH *rollinghashnew(size_t wsiz) {
  if (wsiz <= 2) return NULL;
  ROLLINGHASH *rhash;
  SSMALLOC(rhash, sizeof(ROLLINGHASH));
  rhash->wsiz = wsiz;
  rhash->rtbl = initremovetbl(wsiz);
  return rhash;
}

void rollinghashdel(ROLLINGHASH *rhash) {
  if (rhash == NULL) return;
  SSFREE(rhash->rtbl);
  SSFREE(rhash);
}

uint32_t rollinghashdohash(ROLLINGHASH *rhash, const char *ptr) {
  unsigned int i;
  unsigned char ch0 = (unsigned char)(ptr[0]);
  unsigned char ch1 = (unsigned char)(ptr[1]);
  uint32_t h = (ch0 * RHASHMULT) + ch1;
  for (i = 2; i < rhash->wsiz; i++) {
    unsigned char ch = (unsigned char)(ptr[i]);
    h = MODBASE(h * RHASHMULT + ch);
  }
  return h;
}

uint32_t rollinghashupdate(ROLLINGHASH *rhash,
                           uint32_t oldhash,
                           const char old_first_byte,
                           const char new_last_byte) {
  oldhash += rhash->rtbl[(unsigned int)old_first_byte];
  oldhash += MODBASE((oldhash * RHASHMULT) + ((unsigned char)new_last_byte));
  return oldhash;
}

/*-----------------------------------------------------------------------------
 * private functions
 */

/* rtbl has 256 entry table, used to fast update for rolling.
 * it stores the following value:
 *   rtbl[byte] == (- byte * pow(RHASHMULT, wsiz - 1)) % RHASHBASE
 */
uint32_t *initremovetbl(size_t wsiz) {
  unsigned int i;
  uint32_t *rtbl;
  SSMALLOC(rtbl, sizeof(uint32_t) * 256);

  /* multiplier = pow(RHASHMULT, wsiz - 1) */
  uint32_t multiplier = 1;
  for (i = 0; i < wsiz - 1; i++)
    multiplier = MODBASE(multiplier * RHASHMULT);

  /* byte_multiplier = (byte * multiplier) % RHASHBASE */
  uint32_t byte_multiplier = 0;
  for (i = 0; i < 256; i++) {
    rtbl[i] = MODBASEINV(byte_multiplier);
    byte_multiplier = MODBASE(byte_multiplier + multiplier);
  }
  return rtbl;
}
