#include "quuz.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int g_argc = 0;
char** g_argv = NULL;

int main(int argc, char* argv[])
{
  FILE* fp = stdin;
  enum { PARSE, RUN, EVAL } mode = RUN;
  int debug = 0;

  /* parse options */
  int c;
  while((c = getopt(argc, argv, "pred")) != -1) {
    switch(c) {
      case 'p':
        mode = PARSE;
        break;
      case 'r':
        mode = RUN;
        break;
      case 'e':
        mode = EVAL;
        break;
      case 'd':
        debug = 1;
        break;
    }
  }

  if(optind < argc) {
    if(strcmp(argv[optind], "-") != 0) {
      fp = fopen(argv[optind], "r");
      if(!fp) {
        fputs("could not open input file\n", stderr);
        return EXIT_FAILURE;
      }
      qz_discard_hashbang(fp);
    }
    g_argc = argc - optind;
    g_argv = argv + optind;
  }

  qz_state_t* st = qz_alloc();
  int ret = EXIT_SUCCESS;

  while(!feof(fp)) {
    qz_obj_t obj = qz_read(st, fp);

    if(qz_is_none(obj)) {
      fputs("parsing failed\n", stderr);
      ret = EXIT_FAILURE;
      break;
    }

    if(mode == PARSE) {
      qz_printf(st, st->output_port, "%w\n", obj);
    }
    else if(mode == RUN) {
      qz_unref(st, qz_peval(st, obj));
    }
    else if(mode == EVAL) {
      qz_obj_t result = qz_peval(st, obj);
      if(!qz_is_none(result))
      {
        qz_printf(st, st->output_port, "%w\n", result);
        qz_unref(st, result);
      }
    }

    if(!qz_is_none(st->error_obj)) {
      qz_printf(st, st->error_port, "An error occurred: %w\n", st->error_obj);
      ret = EXIT_FAILURE;
      break;
    }

    qz_unref(st, obj);
  }

  if(debug) {
    qz_printf(st, st->error_port, "st->env = %w\n", st->env);
    qz_printf(st, st->error_port, "st->name_sym = %w\n", st->name_sym);
    qz_printf(st, st->error_port, "st->sym_name = %w\n", st->sym_name);

    fputs("st->root_buffer = \n", stderr);
    for(size_t i = 0; i < st->root_buffer_size; i++)
      fprintf(stderr, " %p\n", (void*)st->root_buffer[i]);
  }

  qz_free(st);

  if(fp != stdin)
    fclose(fp);

  return ret;
}
