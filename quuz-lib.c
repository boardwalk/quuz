#include "quuz.h"
#include <stdlib.h>

#define ALIGNED __attribute__ ((aligned (8)))

static void set_toplevel(qz_state_t* st, const char* name, qz_cfun_t cfun)
{
  qz_set_hash(st, qz_list_head_ptr(qz_list_head(st->env)), qz_make_sym(st, qz_make_string(name)), qz_from_cfun(cfun));
}

static ALIGNED qz_obj_t qz_scm_define(qz_state_t* st, qz_obj_t args)
{
  // (define <variable> <expression>)
  // (define (<variable> <formals> <body>)
  // (define (<variable> . <formal>) <body>)
  qz_pair_t* pair = qz_to_pair(args);

  if(qz_is_sym(pair->first))
  {
    if(!qz_is_pair(pair->rest))
    {
      return qz_error(st, "define given invalid expression argument", args);
    }

    qz_obj_t value = qz_eval(st, qz_to_pair(pair->rest)->first);
    qz_set_hash(st, qz_list_head_ptr(qz_list_head(st->env)), pair->first, value);
  }
  else if(qz_is_pair(pair->first))
  {
    return qz_error(st, "function variant of define not yet implemented", args);
  }
  else
  {
    return qz_error(st, "first argument to define must be a symbol or list", args);
  }

  return QZ_NIL;
}

static ALIGNED qz_obj_t qz_scm_quote(qz_state_t* st, qz_obj_t args)
{
  // (quote a) => a
  return qz_to_pair(args)->first;
}

static ALIGNED qz_obj_t qz_scm_write(qz_state_t* st, qz_obj_t args)
{
  // (write obj)
  // (write obj port)
  qz_obj_t obj = qz_eval(st, qz_to_pair(args)->first);
  qz_write(st, obj, -1, stdout);
  qz_unref(st, obj);
  return QZ_NIL;
}

static ALIGNED qz_obj_t qz_scm_lambda(qz_state_t* st, qz_obj_t args)
{
  // (lambda <formals> <body>)
  qz_cell_t* cell = qz_make_cell(QZ_CT_FUN, 0);

  cell->value.pair.first = qz_ref(st, qz_list_head(st->env));
  cell->value.pair.rest = qz_ref(st, args);

  return qz_from_cell(cell);
}

void qz_init_lib(qz_state_t* st)
{
  set_toplevel(st, "define", qz_scm_define);
  set_toplevel(st, "quote", qz_scm_quote);
  set_toplevel(st, "write", qz_scm_write);
  set_toplevel(st, "lambda", qz_scm_lambda);
}

