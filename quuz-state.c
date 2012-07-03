#include "quuz.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* quuz-lib.c */
qz_obj_t qz_error_handler(qz_state_t*, qz_obj_t);
extern const qz_named_cfun_t QZ_LIB_FUNCTIONS[];

/* quuz-collector.c */
void qz_collect(qz_state_t* st);

static qz_obj_t make_port(qz_state_t* st, int fd, const char* mode)
{
  qz_cell_t* cell = qz_make_cell(QZ_CT_PORT, 0);
  cell->value.fp = fdopen(dup(fd), mode);
  return qz_from_cell(cell);
}

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
  st->input_port = make_port(st, STDIN_FILENO, "r");
  st->output_port = make_port(st, STDOUT_FILENO, "w");
  st->error_port = make_port(st, STDERR_FILENO, "w");
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
  qz_unref(st, st->input_port);
  qz_unref(st, st->output_port);
  qz_unref(st, st->error_port);
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

  int err = setjmp(peval_fail);
  if(!err)
    result = qz_eval(st, obj);

  /* cleanup objects in safety buffers  */
  assert(st->safety_buffer_size >= old_safety_buffer_size);

  for(size_t i = st->safety_buffer_size; i > old_safety_buffer_size; i--)
  {
    qz_obj_t safety_obj = st->safety_buffer[i - 1];

    if(qz_eq(safety_obj, st->env))
      st->env = qz_rest(st->env); /* pop environment */

    qz_unref(st, safety_obj);
  }

  st->safety_buffer_size = old_safety_buffer_size;

  /* handle errors */
  if(err)
  {
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

  /* pop state */
  st->peval_fail = old_peval_fail;

  return result;
}

static qz_obj_t call_function(qz_state_t* st, qz_obj_t fun, qz_obj_t args)
{
  qz_obj_t env = qz_first(fun);
  qz_obj_t params = qz_first(qz_rest(fun));
  qz_obj_t body = qz_rest(qz_rest(fun));

  /* create frame */
  qz_obj_t frame = qz_make_hash();

  while(qz_is_pair(params))
  {
    /* grab parameter */
    qz_obj_t param = qz_first(params);
    params = qz_to_pair(params)->rest;

    if(!qz_is_sym(param)) {
      qz_unref(st, frame);
      return qz_error(st, "function parameter is not a symbol", &param, NULL);
    }

    /* grab argument */
    if(!qz_is_pair(args)) {
      qz_unref(st, frame);
      return qz_error(st, "not enough arguments to function", NULL);
    }

    qz_obj_t arg = qz_first(args);
    args = qz_rest(args);

    qz_push_safety(st, frame);
    arg = qz_eval(st, arg);
    qz_pop_safety(st, 1);

    /* assign argument to parameter */
    qz_hash_set(st, &frame, param, arg);
  }

  if(qz_is_null(params))
  {
    /* all done */
  }
  else if(qz_is_sym(params))
  {
    /* variable parameter, ex. (lambda (a b c . rest) ...) */
    qz_push_safety(st, frame);
    qz_obj_t rest_args = qz_eval_list(st, args);
    qz_pop_safety(st, 1);

    /* assign evaluated arguments list to parameter */
    qz_hash_set(st, &frame, params, rest_args);
  }
  else
  {
    /* it's not a pair, symbol, or null, hmmm... */
    qz_unref(st, frame);
    return qz_error(st, "invalid function formals", &params, NULL);
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

      qz_obj_t result = call_function(st, fun, obj);

      qz_pop_safety(st, 1);
      qz_unref(st, fun);

      return result;
    }
    else if(qz_is_cfun(fun))
    {
      /* no need to unref, this isn't a cell */
      return qz_to_cfun(fun)(st, obj);
    }

    qz_push_safety(st, fun);
    return qz_error(st, "uncallable value", &fun, NULL);
  }

  if(qz_is_sym(obj))
  {
    qz_obj_t* slot = qz_lookup(st, obj);

    if(!slot)
      return qz_error(st, "unbound variable", &obj, NULL);

    return qz_ref(st, *slot);
  }

  if(qz_is_null(obj))
    return qz_error(st, "cannot evaluate null", NULL);

  if(qz_is_none(obj))
    return qz_error(st, "cannot evaluate unspecified value", NULL);

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

qz_obj_t qz_error(qz_state_t* st, const char* msg, ...)
{
  va_list ap;
  va_start(ap, msg);

  qz_obj_t irritants = QZ_NULL;
  qz_obj_t elem;

  for(;;) {
    qz_obj_t* obj = va_arg(ap, qz_obj_t*);

    if(!obj)
      break;

    qz_obj_t inner_elem = qz_make_pair(qz_ref(st, *obj), QZ_NULL);
    if(qz_is_null(irritants)) {
      irritants = elem = inner_elem;
    }
    else {
      qz_to_pair(elem)->rest = inner_elem;
      elem = inner_elem;
    }
  }

  va_end(ap);

  qz_cell_t* cell = qz_make_cell(QZ_CT_ERROR, 0);
  cell->value.pair.first = qz_make_string(msg);
  cell->value.pair.rest = irritants;

  st->error_obj = qz_from_cell(cell);

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

