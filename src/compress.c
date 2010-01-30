#include <ssutil.h>
#include <compress.h>

compressfunc getcompressfunc(int cmethod) {
  switch (cmethod) {
  case SSCMNONE: return sscodec_nonecompress;
  case SSCMZLIB: return sscodec_zlibcompress;
  default:
    assert(false);
    return NULL;
  }
}

decompressfunc getdecompressfunc(int cmethod) {
  switch (cmethod) {
  case SSCMNONE: return sscodec_nonedecompress;
  case SSCMZLIB: return sscodec_zlibdecompress;
  default:
    assert(false);
    return NULL;
  }
}

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
 * ZLIB
 */
#if HAVE_ZLIB
#include <zlib.h>
#define ZLIBBUFSIZ (16*1024)
char *sscodec_zlibcompress(const char *ptr, int size, int *sp) {
  assert(ptr && size >= 0 && sp);
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  if (deflateInit2(&zs, 5, Z_DEFLATED, -15, 7, Z_DEFAULT_STRATEGY) != Z_OK)
    return NULL;
  /*
    if (deflateInit2(&zs, 6, Z_DEFLATED, 15 + 16, 9, Z_DEFAULT_STRATEGY) != Z_OK)
    if (deflateInit2(&zs, 6, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
  */
  int asiz = size + 16;
  if (asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
  char *buf;
  if (!(buf = malloc(asiz))) {
    deflateEnd(&zs);
    return NULL;
  }
  unsigned char obuf[ZLIBBUFSIZ];
  int bsiz = 0;
  zs.next_in = (unsigned char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = ZLIBBUFSIZ;
  int rv;
  while((rv = deflate(&zs, Z_FINISH)) == Z_OK) {
    int osiz = ZLIBBUFSIZ - zs.avail_out;
    if (bsiz + osiz > asiz) {
      asiz = asiz * 2 + osiz;
      char *swap;
      if (!(swap = realloc(buf, asiz))) {
        free(buf);
        deflateEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = ZLIBBUFSIZ;
  }
  if (rv != Z_STREAM_END) {
    free(buf);
    deflateEnd(&zs);
    return NULL;
  }
  int osiz = ZLIBBUFSIZ - zs.avail_out;
  if (bsiz + osiz + 1 > asiz) {
    asiz = asiz * 2 + osiz;
    char *swap;
    if (!(swap = realloc(buf, asiz))) {
      free(buf);
      deflateEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  bsiz++;
  *sp = bsiz;
  deflateEnd(&zs);
  return buf;
}

char *sscodec_zlibdecompress(const char *ptr, int size, int * sp) {
  assert(ptr && size >= 0 && sp);
  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;
  if(inflateInit2(&zs, -15) != Z_OK) return NULL;
  /*
    if(inflateInit2(&zs, 15 + 16) != Z_OK) return NULL;
    if(inflateInit2(&zs, 15) != Z_OK) return NULL;
  */
  int asiz = size * 2 + 16;
  if(asiz < ZLIBBUFSIZ) asiz = ZLIBBUFSIZ;
  char *buf;
  if(!(buf = malloc(asiz))){
    inflateEnd(&zs);
    return NULL;
  }
  unsigned char obuf[ZLIBBUFSIZ];
  int bsiz = 0;
  zs.next_in = (unsigned char *)ptr;
  zs.avail_in = size;
  zs.next_out = obuf;
  zs.avail_out = ZLIBBUFSIZ;
  int rv;
  while((rv = inflate(&zs, Z_NO_FLUSH)) == Z_OK){
    int osiz = ZLIBBUFSIZ - zs.avail_out;
    if(bsiz + osiz >= asiz){
      asiz = asiz * 2 + osiz;
      char *swap;
      if(!(swap = realloc(buf, asiz))){
        free(buf);
        inflateEnd(&zs);
        return NULL;
      }
      buf = swap;
    }
    memcpy(buf + bsiz, obuf, osiz);
    bsiz += osiz;
    zs.next_out = obuf;
    zs.avail_out = ZLIBBUFSIZ;
  }
  if(rv != Z_STREAM_END){
    free(buf);
    inflateEnd(&zs);
    return NULL;
  }
  int osiz = ZLIBBUFSIZ - zs.avail_out;
  if(bsiz + osiz >= asiz){
    asiz = asiz * 2 + osiz;
    char *swap;
    if(!(swap = realloc(buf, asiz))){
      free(buf);
      inflateEnd(&zs);
      return NULL;
    }
    buf = swap;
  }
  memcpy(buf + bsiz, obuf, osiz);
  bsiz += osiz;
  buf[bsiz] = '\0';
  *sp = bsiz;
  inflateEnd(&zs);
  return buf;
}
#else
char *sscodec_zlibcompress(const char *ptr, int size, int *sp) {
  return sscodec_nonecompress(ptr, size, sp);
}

char *sscodec_zlibdecompress(const char *ptr, int size, int * sp) {
  return sscodec_nonedecompress(ptr, size, sp);
}
#endif

/*-----------------------------------------------------------------------------
 * LZO
 */
#if HAVE_LZO
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
#else
char *sscodec_lzocompress(const char *ptr, int size, int *sp) {
  return sscodec_nonecompress(ptr, size, sp);
}

char *sscodec_lzodecompress(const char *ptr, int size, int * sp) {
  return sscodec_nonedecompress(ptr, size, sp);
}
#endif
