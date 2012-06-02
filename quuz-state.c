#include "quuz.h"
#include <stdlib.h>

void qz_init_lib(qz_state_t*); /* quuz-lib.c */

/* create a state */
qz_state_t* qz_alloc()
{
  qz_state_t* st = (qz_state_t*)malloc(sizeof(qz_state_t));
  st->env = qz_make_pair(qz_make_hash(), QZ_NIL);
  st->str_iden = qz_make_hash();
  st->iden_str = qz_make_hash();
  st->next_iden = 0;
  qz_init_lib(st);
  return st;
}

/* free a state */
void qz_free(qz_state_t* st)
{
  qz_unref(st->env);
  qz_unref(st->str_iden);
  qz_unref(st->iden_str);
  free(st);
}

/* execute a form */
qz_obj_t qz_exec(qz_state_t* st, qz_obj_t obj)
{
  if(!qz_is_pair(obj)) {
    fputs("form is not a list: ", stderr);
    qz_write(st, obj, -1, stderr);
    fputc('\n', stderr);
    return QZ_NIL;
  }

  qz_pair_t* pair = qz_to_pair(obj);

  if(!qz_is_identifier(pair->first)) {
    fputs("form does not start with an identifier: ", stderr);
    qz_write(st, obj, -1, stderr);
    fputc('\n', stderr);
  }

  return QZ_NIL;
}

