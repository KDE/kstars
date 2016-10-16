#!/bin/bash
libtoolize -c --force
aclocal -I m4
autoconf -Wall
autoheader
automake --copy --foreign --add-missing
