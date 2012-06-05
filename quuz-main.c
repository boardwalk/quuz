#include "quuz.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
  FILE* fp = stdin;
  enum { UNKNOWN, PARSE, EXEC } mode = UNKNOWN;
  int debug = 0;

  /* parse options */
  int c;
  while((c = getopt(argc, argv, "ped")) != -1) {
    switch(c) {
      case 'p':
        mode = PARSE;
        break;
      case 'e':
        mode = EXEC;
        break;
      case 'd':
        debug = 1;
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

  for(;;) {
    qz_obj_t obj = qz_read(st, fp);

    if(qz_is_nil(obj))
      break;

    if(mode == PARSE)
    {
      qz_write(st, obj, -1, stdout);
    }
    else if(mode == EXEC)
    {
      qz_unref(st, qz_peval(st, obj));
    }

    qz_unref(st, obj);
  }

  if(debug) {
    fputs("st->env = ", stderr);
    qz_write(st, st->env, 6, stderr);
    fputc('\n', stderr);

    fputs("st->name_sym = ", stderr);
    qz_write(st, st->name_sym, -1, stderr);
    fputc('\n', stderr);

    fputs("st->sym_name = ", stderr);
    qz_write(st, st->sym_name, -1, stderr);
    fputc('\n', stderr);

    fputs("st->root_buffer = \n", stderr);
    for(size_t i = 0; i < st->root_buffer_size; i++)
      fprintf(stderr, " %p\n", (void*)st->root_buffer[i]);
  }

  qz_free(st);

  if(fp != stdin)
    fclose(fp);

  return EXIT_SUCCESS;
}

