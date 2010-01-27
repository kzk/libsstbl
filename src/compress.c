#include <ssutil.h>
#include <compress.h>

/*-----------------------------------------------------------------------------
 * none
 */
char *sscodec_nonecompress(const char *ptr, int size, int *sp) {
  if (ptr == NULL || size == 0 || sp == NULL) return NULL;
  char *ret = NULL;
  SSMALLOC(ret, size);
  memcpy(ret, ptr, size);
  *sp = size;
  return ret;
}

char *sscodec_nonedecompress(const char *ptr, int size, int *sp) {
  if (ptr == NULL || size == 0 || sp == NULL) return NULL;
  char *ret = NULL;
  SSMALLOC(ret, size);
  memcpy(ret, ptr, size);
  *sp = size;
  return ret;
}

/*-----------------------------------------------------------------------------
 * LZO
 */
#include <lzo/lzo1x.h>

static int lzo_init = 0;

char *sscodec_lzocompress(const char *ptr, int size, int *sp) {
  assert(ptr && size && sp);
  if (!lzo_init) {
    if (lzo_init() != LZO_E_OK) return NULL;
    lzo_init = 1;
  }
  lzo_bytep buf = NULL;
  SSMALLOC(buf, size + (size >> 4) + 80);
  if (!buf) return NULL;
  lzo_uint bsiz = 0;
  char wrkmem[LZO1X_1_MEM_COMPRESS];
  if (lzo1x_1_compress((const lzo_bytep)ptr, size, buf, &bsiz, wrkmem) != LZO_E_OK) {
    SSFREE(buf);
    return NULL;
  }
  buf[bsiz] = '\0';
  *sp = bsiz;
  return (char *)buf;
}

char *sscodec_lzodecompress(const char *ptr, int size, int * sp) {
  assert(ptr && size && sp);
  if (!lzo_init) {
    if (lzo_init() != LZO_E_OK) return NULL;
    lzo_init = false;
  }
  lzo_bytep buf = NULL;
  lzo_uint bsiz = 0;
  int rat = 6;
  while (true) {
    bsiz = (size + 256) * rat + 3;
    SSMALLOC(buf, bsiz + 1);
    if (!buf) return NULL;
    int rv = lzo1x_decompress_safe((const lzo_bytep)ptr, size, buf, &bsiz, NULL);
    if (rv == LZO_E_OK) {
      break;
    } else if (rv == LZO_E_OUTPUT_OVERRUN) {
      SSFREE(buf);
      rat *= 2;
    } else {
      SSFREE(buf);
      return NULL;
    }
  }
  buf[bsiz] = '\0';
  if (sp) *sp = bsiz;
  return (char *)buf;
}
