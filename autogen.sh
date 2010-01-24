#!/bin/sh

aclocal \
  && autoheader \
  && glibtoolize --force --copy \
  && automake --add-missing --foreign --copy \
  && autoconf
