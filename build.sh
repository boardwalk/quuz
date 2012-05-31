#!/bin/sh
set -e -x
leg quuz.leg > parser.c
flags="-g"
[ "$1" = release ] && flags="-O2 -DNDEBUG"
gcc -D_POSIX_C_SOURCE=200112L -Wall -pedantic -std=c99 $flags -o quuz quuz-main.c quuz-object.c quuz-read.c quuz-hash.c
[ "$1" = release ] && strip -s quuz
