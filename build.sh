#!/bin/sh
set -e -x
leg quuz.leg > parser.c
gcc -D_POSIX_C_SOURCE=200112L -std=c99 -g -o quuz quuz-main.c quuz-cell.c quuz-read.c
