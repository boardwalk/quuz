#include "quuz.h"
#include <stdlib.h>

void qz_init_lib(qz_state_t*); /* quuz-lib.c */

/* lookup the value of an identifier in the current environment */
static qz_obj_t qz_lookup(qz_state_t* st, qz_obj_t iden)
{
  qz_pair_t* scope = qz_to_pair(qz_list_head(st->env));

  for(;;) {
    qz_obj_t value = qz_get_hash(st, scope->first, iden);

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
  st->env = qz_make_pair(qz_make_pair(qz_make_hash(), QZ_NIL), QZ_NIL);
  st->name_sym = qz_make_hash();
  st->sym_name = qz_make_hash();
  st->next_sym = 0;
  st->root_buffer_size = 0;
  qz_init_lib(st);
  return st;
}

/* free a state */
void qz_free(qz_state_t* st)
{
  qz_unref(st, st->env);
  qz_unref(st, st->name_sym);
  qz_unref(st, st->sym_name);
  free(st);
}

/* evaluate an object, top level */
qz_obj_t qz_peval(qz_state_t* st, qz_obj_t obj)
{
  if(setjmp(st->error_handler))
    return QZ_NIL;

  return qz_eval(st, obj);
}

qz_obj_t qz_first(qz_obj_t obj) {
  return qz_to_cell(obj)->value.pair.first;
}

qz_obj_t qz_rest(qz_obj_t obj) {
  return qz_to_cell(obj)->value.pair.rest;
}

/* evaluate an object */
qz_obj_t qz_eval(qz_state_t* st, qz_obj_t obj)
{
  if(qz_is_pair(obj))
  {
    qz_obj_t sym = qz_first(obj);
    if(!qz_is_sym(sym))
      return qz_error(st, "list does not start with an symbol", obj);

    qz_obj_t value = qz_lookup(st, sym);
    if(qz_is_nil(value))
      return qz_error(st, "unbound variable", obj);

    if(qz_is_fun(value))
    {
      qz_obj_t env = qz_first(value);
      qz_obj_t formals = qz_first(qz_rest(value));
      qz_obj_t body = qz_first(qz_rest(qz_rest(value)));

      /* push environment */
      qz_obj_t old_env = st->env;
      st->env = qz_make_pair(qz_ref(st, env), qz_ref(st, st->env));

      /* TODO bind arguments */

      /* execute function */
      qz_obj_t result = qz_eval(st, body);

      /* pop environment */
      qz_unref(st, st->env);
      st->env = old_env;

      return result;
    }
    else if(qz_is_cfun(value))
    {
      return qz_to_cfun(value)(st, qz_rest(obj));
    }

    return qz_error(st, "uncallable value", obj);
  }

  if(qz_is_sym(obj))
  {
    qz_obj_t value = qz_lookup(st, obj);

    if(qz_is_nil(value))
      return qz_error(st, "unbound variable", obj);

    return qz_ref(st, value);
  }

  return qz_ref(st, obj);
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

