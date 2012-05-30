#include "quuz.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  FILE* fp = stdin;
  enum { UNKNOWN, PARSE } mode = UNKNOWN;

  /* parse options */
  int c;
  while((c = getopt(argc, argv, "p")) != -1) {
    switch(c) {
      case 'p':
        mode = PARSE;
        break;
    }
  }

  if(mode == UNKNOWN) {
    fputs("mode option required\n", stderr);
    return EXIT_FAILURE;
  }

  if(optind < argc) {
    fp = fopen(argv[optind], "r");
    if(!fp) {
      fputs("could not open input file\n", stderr);
      return EXIT_FAILURE;
    }
  }

  qz_obj_t obj = qz_read(fp);

  if(qz_is_nil(obj))
    return EXIT_FAILURE;

  if(mode == PARSE)
    qz_write(obj, -1, stdout);

  qz_destroy(obj);

  return EXIT_SUCCESS;
}

