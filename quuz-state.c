#include "quuz.h"
#include <stdlib.h>

void qz_init_lib(qz_state_t*); /* quuz-lib.c */

/* lookup the value of an identifier in the current environment */
static qz_obj_t qz_lookup(qz_state_t* st, qz_obj_t iden)
{
  qz_pair_t* scope = qz_to_pair(st->env);

  for(;;) {
    qz_obj_t value = qz_get_hash(scope->first, iden);

    if(!qz_is_nil(value))
      return value;

    if(qz_is_nil(scope->rest))
      return QZ_NIL;

    scope = qz_to_pair(scope->rest);
  }
}

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

/* evaluate an object, top level */
qz_obj_t qz_peval(qz_state_t* st, qz_obj_t obj)
{
  if(setjmp(st->error_handler))
    return QZ_NIL;

  return qz_eval(st, obj);
}

/* evaluate an object */
qz_obj_t qz_eval(qz_state_t* st, qz_obj_t obj)
{
  if(qz_is_pair(obj))
  {
    qz_pair_t* pair = qz_to_pair(obj);

    if(!qz_is_identifier(pair->first))
      return qz_error(st, "list does not start with an identifier", obj);

    qz_obj_t value = qz_lookup(st, pair->first);

    if(qz_is_nil(value))
      return qz_error(st, "unbound variable", obj);

    if(!qz_is_cfn(value))
      return qz_error(st, "uncallable value", obj);

    qz_cfn_t cfn = qz_to_cfn(value);
    return cfn(st, pair->rest);
  }

  if(qz_is_identifier(obj))
  {
    qz_obj_t value = qz_lookup(st, obj);

    if(qz_is_nil(value))
      return qz_error(st, "unbound variable", obj);

    return value;
  }

  return obj;
}

/* throw an error. doesn't return */
qz_obj_t qz_error(qz_state_t* st, const char* msg, qz_obj_t context)
{
  fputs("Error: ", stderr);
  fputs(msg, stderr);
  fputc('\n', stderr);

  fputs("Context: ", stderr);
  qz_write(st, context, -1, stderr);
  fputc('\n', stderr);

  longjmp(st->error_handler, 1);
}

