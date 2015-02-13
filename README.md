# quuz

quuz is a embeddable Scheme interpreter in the style of lua.

[![Build Status](https://travis-ci.org/boardwalk/quuz.svg?branch=master)](https://travis-ci.org/boardwalk/quuz)

## Building

Requires [peg](http://piumarta.com/software/peg/).

```bash
./build.sh
```

## Testing

Requires [Test::Base](http://search.cpan.org/~ingy/Test-Base-0.88/lib/Test/Base.pod), [File::Which](http://search.cpan.org/~pereinar/File-Which-0.05/Which.pm).

Optionally uses [valgrind](http://valgrind.org).

```bash
prove -Iperllib
```
