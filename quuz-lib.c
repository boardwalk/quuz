#include "quuz.h"

static void set_toplevel(qz_state_t* st, const char* iden, qz_cfn_t cfn)
{
  qz_set_hash(qz_list_tail(st->env), qz_make_iden(st, qz_make_string(iden)), qz_make_cfn(cfn));
}

static qz_obj_t qz_scm_define(qz_state_t* st, qz_obj_t args)
{
  // (define <variable> <expression>)
  // (define (<variable> <formals> <body>)
  // (define (<variable> . <formal>) <body>)
  qz_pair_t* pair = qz_to_pair(args);

  if(qz_is_identifier(pair->first))
  {
    if(!qz_is_pair(pair->rest))
    {
      return qz_error(st, "define given invalid expression argument", args);
    }

    qz_obj_t value = qz_eval(st, qz_to_pair(pair->rest)->first);
    qz_set_hash(qz_list_head(st->env), pair->first, value);
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

static qz_obj_t qz_scm_quote(qz_state_t* st, qz_obj_t args)
{
  // (quote a) => a
  return qz_to_pair(args)->first;
}

static qz_obj_t qz_scm_write(qz_state_t* st, qz_obj_t args)
{
  // (write obj)
  // (write obj port)
  qz_obj_t obj = qz_eval(st, qz_to_pair(args)->first);
  qz_write(st, obj, -1, stdout);
  return QZ_NIL;
}

void qz_init_lib(qz_state_t* st)
{
  set_toplevel(st, "define", qz_scm_define);
  set_toplevel(st, "quote", qz_scm_quote);
  set_toplevel(st, "write", qz_scm_write);
}

