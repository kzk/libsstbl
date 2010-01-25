#include <ssutil.h>

#include <unistd.h>
#include <stdint.h>

int sswrite(int fd, const void *buf, size_t size) {
  assert(fd >= 0 && buf && size);
  size_t nbytes = 0;
  const char *p = buf;
  const char *endp = p + size;
  while (p < endp) {
    ssize_t n = -1;
    SSSYS_NOINTR(n, write(fd, p, endp - p));
    if (n < 0) {
      return -1;
    } else if (n == 0) {
      break;
    } else {
      nbytes += n;
      p += n;
    }
  }
  return (nbytes == size) ? 0 : -1;
}

int ssread(int fd, void *buf, size_t size) {
  assert(fd >= 0 && buf && size);
  size_t nbytes = 0;
  char *p = buf;
  const char *endp = p + size;
  while (p < endp) {
    ssize_t n = -1;
    SSSYS_NOINTR(n, read(fd, p, endp - p));
    if (n < 0) {
      return -1;
    } else if (n == 0) {
      break;
    } else {
      nbytes += n;
      p += n;
    }
  }
  return (nbytes == size) ? 0 : -1;
}
