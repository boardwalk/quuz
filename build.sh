#!/bin/sh
set -e -x

leg quuz.leg > parser.c || exit 1

flags="-g"
[ "$1" = release ] && flags="-O2 -fno-asynchronous-unwind-tables -DNDEBUG"

flags="$flags -Wall -Wextra -Wno-sign-compare -pedantic"

g++ $flags -std=c++0x -c \
  city.cc

gcc -D_POSIX_C_SOURCE=200809L $flags -std=c99 -o quuz \
  quuz-main.c \
  quuz-object.c \
  quuz-collector.c \
  quuz-read.c \
  quuz-write.c \
  quuz-hash.c \
  city.o \
  quuz-state.c \
  quuz-lib.c \
  quuz-util.c || exit 1

[ "$1" = release ] && strip -s quuz

exit 0
