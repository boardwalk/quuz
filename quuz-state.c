#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* quuz-lib.c */
qz_obj_t qz_error_handler(qz_state_t*, qz_obj_t);
extern const qz_named_cfun_t QZ_LIB_FUNCTIONS[];

/* quuz-collector.c */
void qz_collect(qz_state_t* st);

qz_state_t* qz_alloc(void)
{
  qz_state_t* st = (qz_state_t*)malloc(sizeof(qz_state_t));
  st->root_buffer_size = 0;
  st->safety_buffer_size = 0;
  st->peval_fail = NULL;
  st->error_handler = qz_from_cfun(qz_error_handler);
  st->error_obj = QZ_NONE;
  qz_obj_t toplevel = qz_make_hash();
  st->env = qz_make_pair(qz_make_pair(toplevel, QZ_NULL), QZ_NULL);
  /*fprintf(stderr, "toplevel = %p\n", (void*)qz_to_cell(toplevel));*/
  st->name_sym = qz_make_hash();
  /*fprintf(stderr, "name_sym = %p\n", (void*)qz_to_cell(st->name_sym));*/
  st->sym_name = qz_make_hash();
  /*fprintf(stderr, "sym_name = %p\n", (void*)qz_to_cell(st->sym_name));*/
  st->next_sym = 1;
  st->begin_sym = qz_make_sym(st, qz_make_string("begin"));
  st->else_sym = qz_make_sym(st, qz_make_string("else"));
  st->arrow_sym = qz_make_sym(st, qz_make_string("=>"));
  st->quote_sym = qz_make_sym(st, qz_make_string("quote"));
  st->quasiquote_sym = qz_make_sym(st, qz_make_string("quasiquote"));
  st->unquote_sym = qz_make_sym(st, qz_make_string("unquote"));
  st->unquote_splicing_sym = qz_make_sym(st, qz_make_string("unquote-splicing"));
  st->args_sym = qz_make_sym(st, qz_make_string("args"));

  for(const qz_named_cfun_t* ncf = QZ_LIB_FUNCTIONS; ncf->cfun; ncf++)
  {
    qz_hash_set(st, qz_list_head_ptr(qz_list_head(st->env)),
        qz_make_sym(st, qz_make_string(ncf->name)), qz_from_cfun(ncf->cfun));
  }

  return st;
}

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

qz_obj_t qz_peval(qz_state_t* st, qz_obj_t obj)
{
  /* push state */
  jmp_buf peval_fail;
  jmp_buf* old_peval_fail = st->peval_fail;
  st->peval_fail = &peval_fail;

  size_t old_safety_buffer_size = st->safety_buffer_size;

  /* call eval() protected by setjmp/longjmp */
  qz_obj_t result;

  if(setjmp(peval_fail))
  {
    /* cleanup objects in safety buffer */
    assert(st->safety_buffer_size >= old_safety_buffer_size);

    for(size_t i = old_safety_buffer_size; i < st->safety_buffer_size; i++)
    {
      qz_obj_t safety_obj = st->safety_buffer[i];

      if(qz_eq(safety_obj, st->env))
        st->env = qz_rest(st->env); /* pop environment */

      qz_unref(st, safety_obj);
    }

    st->safety_buffer_size = old_safety_buffer_size;

    /* push failsafe error handler */
    qz_obj_t old_handler = st->error_handler;
    st->error_handler = qz_from_cfun(qz_error_handler);

    /* call handler */
    qz_obj_t handler_call = qz_make_pair(qz_ref(st, old_handler), qz_make_pair(st->error_obj, QZ_NULL));
    result = qz_peval(st, handler_call);
    qz_unref(st, handler_call);

    /* pop failsafe error handler */
    st->error_handler = old_handler;
  }
  else
  {
    result = qz_eval(st, obj);
    assert(st->safety_buffer_size == old_safety_buffer_size);
  }

  /* pop state */
  st->peval_fail = old_peval_fail;

  return result;
}

qz_obj_t qz_eval(qz_state_t* st, qz_obj_t obj)
{
  if(qz_is_pair(obj))
  {
    qz_obj_t fun = qz_eval(st, qz_required_arg(st, &obj));

    if(qz_is_fun(fun))
    {
      qz_push_safety(st, fun);

      qz_obj_t env = qz_first(fun);
      qz_obj_t formals = qz_first(qz_rest(fun));
      qz_obj_t body = qz_rest(qz_rest(fun));

      /* create frame */
      qz_obj_t frame = qz_make_hash();

      for(;;) {
        qz_obj_t param = qz_optional_arg(st, &formals);

        if(qz_is_none(param))
          break; /* ran out of params */

        qz_push_safety(st, frame);
        qz_obj_t arg = qz_eval(st, qz_required_arg(st, &obj));
        qz_pop_safety(st, 1);

        qz_hash_set(st, &frame, param, arg);
      }

      /* push environment with frame */
      qz_obj_t old_env = st->env;
      st->env = qz_make_pair(qz_make_pair(frame, qz_ref(st, env)), qz_ref(st, st->env));
      qz_push_safety(st, st->env);

      /* execute function */
      qz_obj_t result = qz_eval(st, body);

      /* pop environment */
      qz_pop_safety(st, 1);
      qz_unref(st, st->env);
      st->env = old_env;

      qz_pop_safety(st, 1);
      qz_unref(st, fun);
      return result;
    }
    else if(qz_is_cfun(fun))
    {
      /* no need to unref, this isn't a cell */
      return qz_to_cfun(fun)(st, obj);
    }

    qz_unref(st, fun);
    return qz_error(st, "uncallable value");
  }

  if(qz_is_sym(obj))
  {
    qz_obj_t* slot = qz_lookup(st, obj);

    if(!slot)
      return qz_error(st, "unbound variable");

    return qz_ref(st, *slot);
  }

  if(qz_is_null(obj))
    return qz_error(st, "cannot evaluate null");

  if(qz_is_none(obj))
    return qz_error(st, "cannot evaluate unspecified value");

  return qz_ref(st, obj);
}

qz_obj_t* qz_lookup(qz_state_t* st, qz_obj_t sym)
{
  qz_pair_t* scope = qz_to_pair(qz_list_head(st->env));

  for(;;) {
    qz_obj_t* slot = qz_hash_get(st, scope->first, sym);

    if(slot)
      return slot;

    if(qz_is_null(scope->rest))
      return NULL;

    scope = qz_to_pair(scope->rest);
  }
}

qz_obj_t qz_error(qz_state_t* st, const char* msg)
{
  st->error_obj = qz_make_string(msg);
  longjmp(*st->peval_fail, 1);
}

void qz_push_safety(qz_state_t* st, qz_obj_t obj)
{
  assert(st->safety_buffer_size < QZ_SAFETY_BUFFER_CAPACITY);
  st->safety_buffer[st->safety_buffer_size++] = obj;
}

void qz_pop_safety(qz_state_t* st, size_t nobj)
{
  assert(st->safety_buffer_size >= nobj);
  st->safety_buffer_size -= nobj;
}

