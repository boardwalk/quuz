#!/bin/sh
set -e -x
VERSION=0.1.15
wget "http://piumarta.com/software/peg/peg-$VERSION.tar.gz"
tar xzf peg-$VERSION.tar.gz
cd peg-$VERSION
make
sudo make install