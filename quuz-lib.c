#include "quuz.h"

static void set_toplevel(qz_state_t* st, const char* iden, qz_cfn_t cfn)
{
  qz_set_hash(qz_list_tail(st->env), qz_make_iden(st, qz_make_string(iden)), qz_make_cfn(cfn));
}

static qz_obj_t qz_scm_define(qz_state_t* st, qz_obj_t args)
{
  // (define <variable> <expression>)
  // (define (<variable> <formals> <body>)
  // (define (<variable> . formal) <body>)
  return QZ_NIL;
}

void qz_init_lib(qz_state_t* st)
{
  set_toplevel(st, "define", qz_scm_define);
}

