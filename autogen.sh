#!/bin/sh
# script to prepare PearPC sources
aclocal -I . \
&& autoheader \
&& automake --add-missing \
&& autoconf \
|| exit 1

echo PearPC sources are now prepared. To build here, run:
echo " ./configure"
echo " make"
