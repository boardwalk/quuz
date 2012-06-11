#include "quuz.h"
#include <stdlib.h>

void qz_init_lib(qz_state_t*); /* quuz-lib.c */
extern const qz_named_cfun_t QZ_LIB_FUNCTIONS[]; /* quuz-lib2.c */

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
qz_state_t* qz_alloc(void)
{
  qz_state_t* st = (qz_state_t*)malloc(sizeof(qz_state_t));
  st->root_buffer_size = 0;
  qz_obj_t toplevel = qz_make_hash();
  st->env = qz_make_pair(qz_make_pair(toplevel, QZ_NIL), QZ_NIL);
  /*fprintf(stderr, "toplevel = %p\n", (void*)qz_to_cell(toplevel));*/
  st->name_sym = qz_make_hash();
  /*fprintf(stderr, "name_sym = %p\n", (void*)qz_to_cell(st->name_sym));*/
  st->sym_name = qz_make_hash();
  /*fprintf(stderr, "sym_name = %p\n", (void*)qz_to_cell(st->sym_name));*/
  st->next_sym = 1;
  st->else_sym = qz_make_sym(st, qz_make_string("else"));
  st->arrow_sym = qz_make_sym(st, qz_make_string("=>"));

  for(const qz_named_cfun_t* ncf = QZ_LIB_FUNCTIONS; ncf->cfun; ncf++)
  {
    qz_set_hash(st, qz_list_head_ptr(qz_list_head(st->env)),
        qz_make_sym(st, qz_make_string(ncf->name)), qz_from_cfun(ncf->cfun));
  }
  qz_init_lib(st);

  return st;
}

/* free a state */
void qz_free(qz_state_t* st)
{
  /*fprintf(stderr, "destroying env...\n");*/
  qz_unref(st, st->env);
  /*fprintf(stderr, "destroying name_sym...\n");*/
  qz_unref(st, st->name_sym);
  /*fprintf(stderr, "destroying sym_name...\n");*/
  qz_unref(st, st->sym_name);
  qz_collect(st);
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
    qz_obj_t fun = qz_eval(st, qz_required_arg(st, &obj));

    if(qz_is_fun(fun))
    {
      qz_obj_t env = qz_first(fun);
      qz_obj_t formals = qz_first(qz_rest(fun));
      qz_obj_t body = qz_first(qz_rest(qz_rest(fun)));

      /* bind arguments */
      qz_obj_t fun_env = qz_make_hash();

      for(;;) {
        qz_obj_t param = qz_optional_arg(st, &formals);

        if(qz_is_nil(param))
          break; /* ran out of params */

        /* TODO cleanup fun and fun_env if this fails */
        qz_obj_t arg = qz_eval(st, qz_required_arg(st, &obj));

        qz_set_hash(st, &fun_env, param, arg);
      }

      /* push environment */
      qz_obj_t old_env = st->env;
      st->env = qz_make_pair(qz_make_pair(fun_env, qz_ref(st, env)), qz_ref(st, st->env));

      /* execute function */
      /* TODO cleanup fun and env if this fails */
      qz_obj_t result = qz_eval(st, body);

      /* pop environment */
      qz_unref(st, st->env);
      st->env = old_env;

      qz_unref(st, fun);
      return result;
    }
    else if(qz_is_cfun(fun))
    {
      /* no need to unref, this isn't a cell */
      return qz_to_cfun(fun)(st, obj);
    }

    /* TODO unref fun */
    return qz_error(st, "uncallable value", fun);
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
  qz_write(st, context, 5, stderr);
  fputc('\n', stderr);

  longjmp(st->error_handler, 1);
}

