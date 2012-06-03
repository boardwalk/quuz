#include "quuz.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  FILE* fp = stdin;
  enum { UNKNOWN, PARSE, EXEC } mode = UNKNOWN;

  /* parse options */
  int c;
  while((c = getopt(argc, argv, "pe")) != -1) {
    switch(c) {
      case 'p':
        mode = PARSE;
        break;
      case 'e':
        mode = EXEC;
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

  qz_state_t* st = qz_alloc();

  qz_obj_t obj = qz_read(st, fp);

  if(mode == PARSE)
    qz_write(st, obj, -1, stdout);
  else if(mode == EXEC)
    qz_peval(st, obj);

  qz_unref(obj);
  qz_free(st);

  return EXIT_SUCCESS;
}

