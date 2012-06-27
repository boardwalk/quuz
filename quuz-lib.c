#include "quuz.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define ALIGNED __attribute__ ((aligned (8)))
#define QZ_DEF_CFUN(n) static ALIGNED qz_obj_t n(qz_state_t* st, qz_obj_t args)

typedef int (*pred_fun)(qz_obj_t);
typedef int (*cmp_fun)(qz_obj_t, qz_obj_t);

static void set_var(qz_state_t* st, qz_obj_t name, qz_obj_t value)
{
  qz_obj_t outer_env = qz_to_cell(st->env)->value.pair.first;
  qz_obj_t* inner_env = &qz_to_cell(outer_env)->value.pair.first;
  qz_hash_set(st, inner_env, name, value);
}

qz_obj_t predicate(qz_state_t* st, qz_obj_t args, pred_fun pf)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));
  int b = pf(value);
  qz_unref(st, value);
  return b ? QZ_TRUE : QZ_FALSE;
}

#define LESS 1
#define EQUAL 2
#define GREATER 4

static int sign_of(int i)
{
  return 2 + 2*(i > 0) - (i < 0);
}

static qz_obj_t compare_many(qz_state_t* st, qz_obj_t args, pred_fun pf, cmp_fun cf, int flags)
{
  qz_obj_t prev = qz_eval(st, qz_required_arg(st, &args));
  if(!pf(prev)) {
    qz_unref(st, prev);
    return qz_error(st, "unexpected type");
  }

  /* compare each pair of objects */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);
    if(qz_is_none(expr)) {
      qz_unref(st, prev);
      return QZ_TRUE; /* ran out of expressions */
    }

    qz_push_safety(st, prev);

    qz_obj_t curr = qz_eval(st, expr);
    if(!pf(curr)) {
      qz_unref(st, curr);
      return qz_error(st, "unexpected type");
    }

    qz_pop_safety(st, 1);

    if(!(sign_of(cf(prev, curr)) & flags)) {
      qz_unref(st, prev);
      qz_unref(st, curr);
      return QZ_FALSE; /* comparison didn't match */
    }

    qz_unref(st, prev);
    prev = curr;
  }
}

/* generic function for string-ref, vector-ref, and bytevector-ref */
typedef qz_obj_t (*getelem_fun)(qz_state_t*, qz_cell_t*, size_t);

static qz_obj_t array_ref(qz_state_t* st, qz_obj_t args, const char* type_spec, getelem_fun gef)
{
  qz_obj_t obj, k;
  qz_get_args(st, &args, type_spec, &obj, &k);

  intptr_t k_raw = qz_to_fixnum(k);

  qz_cell_t* cell = qz_to_cell(obj);
  if(k_raw < 0 || k_raw >= cell->value.array.size) {
    qz_unref(st, obj);
    return qz_error(st, "index out of bounds");
  }

  qz_obj_t result = gef(st, cell, k_raw);
  qz_unref(st, obj);
  return result;
}

/* generic function for string-set!, vector-set!, and bytevector-set! */
typedef void (*setelem_fun)(qz_state_t*, qz_cell_t*, size_t, qz_obj_t);

static qz_obj_t array_set(qz_state_t* st, qz_obj_t args, const char* type_spec, setelem_fun sef)
{
  /* TODO add immutable flag to strings,
   * set for strings converted from symbols
   * check here and elsewhere and throw an error */

  qz_obj_t arr, k, obj;
  qz_get_args(st, &args, type_spec, &arr, &k, &obj);

  intptr_t k_raw = qz_to_fixnum(k);

  qz_cell_t* cell = qz_to_cell(arr);
  if(k_raw < 0 || k_raw >= cell->value.array.size) {
    qz_unref(st, arr);
    return qz_error(st, "index out of bounds");
  }

  sef(st, cell, k_raw, obj);
  qz_unref(st, arr);
  return QZ_NONE;
}



QZ_DEF_CFUN(scm_begin);

/******************************************************************************
 * 4.1. Primitive expression types
 ******************************************************************************/

/* 4.1.2. Literal expressions */
QZ_DEF_CFUN(scm_quote)
{
  return qz_ref(st, qz_required_arg(st, &args));
}

/* 4.1.4. Procedures */
QZ_DEF_CFUN(scm_lambda)
{
  qz_obj_t formals = qz_ref(st, qz_required_arg(st, &args));
  qz_obj_t body = qz_make_pair(st->begin_sym, qz_ref(st, args));

  qz_cell_t* cell = qz_make_cell(QZ_CT_FUN, 0);

  cell->value.pair.first = qz_ref(st, qz_list_head(st->env));
  cell->value.pair.rest = qz_make_pair(formals, body);

  return qz_from_cell(cell);
}

/* 4.1.5. Conditionals */
QZ_DEF_CFUN(scm_if)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t consequent = qz_required_arg(st, &args);
  qz_obj_t alternate = qz_optional_arg(st, &args);

  qz_obj_t test_result = qz_eval(st, test);

  if(qz_eq(test_result, QZ_FALSE))
    return qz_eval(st, alternate); /* covers no alternate, none evals to none */

  qz_unref(st, test_result);

  return qz_eval(st, consequent);
}

/* 4.1.6. Assignments */
QZ_DEF_CFUN(scm_set_b)
{
  qz_obj_t var = qz_required_arg(st, &args);
  qz_obj_t expr = qz_required_arg(st, &args);

  qz_obj_t* slot = qz_lookup(st, var);

  if(!slot)
    qz_error(st, "unbound variable in set!");

  qz_obj_t value = qz_eval(st, expr);

  qz_unref(st, *slot);
  *slot = value;

  return QZ_NONE;
}

/******************************************************************************
 * 4.2. Derived expression types
 ******************************************************************************/

/* 4.2.1. Conditions */
QZ_DEF_CFUN(scm_cond)
{
  qz_obj_t clause;
  qz_obj_t result = QZ_NONE;

  /* find matching clause */
  for(;;) {
    clause = qz_optional_arg(st, &args);

    if(qz_is_none(clause))
      return QZ_NONE; /* ran out of clauses */

    qz_obj_t test = qz_required_arg(st, &clause);

    if(qz_eq(test, st->else_sym))
      break; /* hit else clause */

    /* no need to unref previous result, it must be none or false */
    result = qz_eval(st, test);

    if(!qz_eq(result, QZ_FALSE))
      break; /* hit true clause */
  }

  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_is_none(expr))
    return result; /* clause with only test */

  if(qz_eq(expr, st->arrow_sym)) {
    /* get function */
    qz_push_safety(st, result);
    qz_obj_t fun = qz_required_arg(st, &clause);
    qz_pop_safety(st, 1);
    /* call function */
    qz_obj_t fun_call = qz_make_pair(qz_ref(st, fun), qz_make_pair(result, QZ_NULL));
    qz_push_safety(st, fun_call);
    result = qz_eval(st, fun_call);
    qz_pop_safety(st, 1);
    qz_unref(st, fun_call);
    return result;
  }

  /* eval expressions in clause */
  for(;;) {
    qz_unref(st, result); /* from test or previous expression */
    result = qz_eval(st, expr);

    expr = qz_optional_arg(st, &clause);
    if(qz_is_none(expr))
      return result; /* ran out of expressions in clause */
  }
}

QZ_DEF_CFUN(scm_case)
{
  qz_obj_t result = qz_eval(st, qz_required_arg(st, &args));

  /* find matching clause */
  qz_obj_t clause;

  for(;;) {
    clause = qz_optional_arg(st, &args);

    if(qz_is_none(clause))
      break; /* ran out of clauses */

    if(qz_eq(clause, st->else_sym))
      break; /* hit else clause */

    qz_obj_t datum_list = qz_required_arg(st, &clause);

    /* match against each datum in list */
    for(;;) {
      qz_obj_t datum = qz_optional_arg(st, &datum_list);

      if(qz_is_none(datum)) {
        clause = QZ_NONE;
        break; /* ran out of datum */
      }

      if(qz_eqv(datum, result))
        break; /* hit matching clause */
    }
    if(!qz_is_none(clause))
      break; /* hit matching clause */
  }

  if(qz_is_none(clause)) {
    qz_unref(st, result);
    return QZ_NONE; /* no clause found */
  }

  /* eval expressions in clause */
  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_eq(expr, st->arrow_sym)) {
    /* get function */
    qz_push_safety(st, result);
    qz_obj_t fun = qz_required_arg(st, &clause);
    qz_pop_safety(st, 1);
    /* call function */
    qz_obj_t fun_call = qz_make_pair(qz_ref(st, fun), qz_make_pair(result, QZ_NULL));
    qz_push_safety(st, fun_call);
    result = qz_eval(st, fun_call);
    qz_pop_safety(st, 1);
    qz_unref(st, fun_call);
    return result;
  }

  for(;;) {
    if(qz_is_none(expr))
      return result; /* ran out of expressions in clause */

    qz_unref(st, result); /* from previous expression */
    result = qz_eval(st, expr);

    expr = qz_optional_arg(st, &clause);
  }
}

QZ_DEF_CFUN(scm_and)
{
  qz_obj_t result = QZ_TRUE;

  /* eval tests */
  for(;;) {
    qz_obj_t test = qz_optional_arg(st, &args);

    if(qz_is_none(test))
      return result; /* ran out of tests */

    qz_unref(st, result); /* from previous test */
    result = qz_eval(st, test);

    if(qz_eq(result, QZ_FALSE))
      return QZ_FALSE; /* not all expressions true */
  }
}

QZ_DEF_CFUN(scm_or)
{
  /* eval tests */
  for(;;) {
    qz_obj_t test = qz_optional_arg(st, &args);

    if(qz_is_none(test))
      return QZ_FALSE; /* ran out of tests */

    qz_obj_t result = qz_eval(st, test);

    if(!qz_eq(result, QZ_FALSE))
      return result; /* not all expressions false */
  }
}

QZ_DEF_CFUN(scm_when)
{
  qz_obj_t result = qz_eval(st, qz_required_arg(st, &args));

  if(qz_eq(result, QZ_FALSE))
    return QZ_NONE; /* test was false */

  qz_unref(st, result);

  /* eval expressions */
  qz_unref(st, scm_begin(st, args));

  return QZ_NONE;
}

QZ_DEF_CFUN(scm_unless)
{
  qz_obj_t result = qz_eval(st, qz_required_arg(st, &args));

  if(!qz_eq(result, QZ_FALSE)) {
    qz_unref(st, result);
    return QZ_NONE; /* test was true */
  }

  /* eval expressions */
  qz_unref(st, scm_begin(st, args));

  return QZ_NONE;
}

/* 4.2.2. Binding constructs */
QZ_DEF_CFUN(scm_let)
{
  qz_obj_t bindings = qz_required_arg(st, &args);

  /* create frame */
  qz_obj_t frame = qz_make_hash();

  for(;;) {
    qz_obj_t binding = qz_optional_arg(st, &bindings);

    if(qz_is_none(binding))
      break;

    qz_push_safety(st, frame);

    qz_obj_t sym = qz_required_arg(st, &binding);
    qz_obj_t expr = qz_required_arg(st, &binding);

    if(!qz_is_sym(sym))
      qz_error(st, "expected symbol");

    qz_hash_set(st, &frame, sym, qz_eval(st, expr));

    qz_pop_safety(st, 1);
  }

  /* push environment with frame */
  qz_obj_t old_env = st->env;
  qz_obj_t env = qz_make_pair(frame, qz_ref(st, qz_first(st->env)));
  st->env = qz_make_pair(env, qz_ref(st, st->env));
  qz_push_safety(st, st->env);

  /* execute body */
  qz_obj_t result = scm_begin(st, args);

  /* pop environment */
  qz_pop_safety(st, 1);
  qz_unref(st, st->env);
  st->env = old_env;

  return result;
}

QZ_DEF_CFUN(scm_let_s)
{
  qz_obj_t bindings = qz_required_arg(st, &args);

  /* push environment with frame */
  qz_obj_t old_env = st->env;
  qz_obj_t env = qz_make_pair(qz_make_hash(), qz_ref(st, qz_first(st->env)));
  st->env = qz_make_pair(env, qz_ref(st, st->env));
  qz_push_safety(st, st->env);

  /* fill frame while binding */
  for(;;) {
    qz_obj_t binding = qz_optional_arg(st, &bindings);

    if(qz_is_none(binding))
      break;

    qz_obj_t sym = qz_required_arg(st, &binding);
    qz_obj_t expr = qz_required_arg(st, &binding);

    if(!qz_is_sym(sym))
      qz_error(st, "expected symbol");

    qz_hash_set(st, &qz_to_pair(env)->first, sym, qz_eval(st, expr));
  }

  /* execute body */
  qz_obj_t result = scm_begin(st, args);

  /* pop environment */
  qz_pop_safety(st, 1);
  qz_unref(st, st->env);
  st->env = old_env;

  return result;
}

/* 4.2.3. Sequencing */
QZ_DEF_CFUN(scm_begin)
{
  qz_obj_t result = QZ_NONE;

  /* eval expressions */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);

    if(qz_is_none(expr))
      return result; /* ran out of expressions */

    qz_unref(st, result);
    result = qz_eval(st, expr);
  }
}
/* 4.2.5. Delayed evaluation */
QZ_DEF_CFUN(scm_delay)
{
  qz_obj_t expr = qz_required_arg(st, &args);

  qz_cell_t* cell = qz_make_cell(QZ_CT_PROMISE, 0);
  cell->value.pair.first = qz_ref(st, qz_first(st->env));
  cell->value.pair.rest = qz_ref(st, expr);

  return qz_from_cell(cell);
}

QZ_DEF_CFUN(scm_lazy)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "q", &obj);

  return obj;
}

QZ_DEF_CFUN(scm_force)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "q", &obj);

  qz_pair_t* pair = qz_to_promise(obj);

  if(qz_is_none(pair->first)) {
    qz_obj_t result = qz_ref(st, pair->rest);
    qz_unref(st, obj);
    return result;
  }

  qz_push_safety(st, obj);

  /* push environment */
  qz_obj_t old_env = st->env;
  st->env = qz_make_pair(qz_ref(st, pair->first), qz_ref(st, st->env));
  qz_push_safety(st, st->env);

  /* evaluate expression */
  qz_obj_t result = qz_eval(st, pair->rest);

  /* pop environment */
  qz_pop_safety(st, 1);
  qz_unref(st, st->env);
  st->env = old_env;

  /* replace unforced promise with forced promise */
  qz_unref(st, pair->first);
  qz_unref(st, pair->rest);
  pair->first = QZ_NONE;
  pair->rest = qz_ref(st, result);

  qz_pop_safety(st, 1);
  qz_unref(st, obj);

  return result;
}

QZ_DEF_CFUN(scm_eager)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "a", &obj);

  qz_cell_t* cell = qz_make_cell(QZ_CT_PROMISE, 0);
  cell->value.pair.first = QZ_NONE;
  cell->value.pair.rest = obj;

  return qz_from_cell(cell);
}

/* 4.2.8. Quasiquotation */

static qz_obj_t list_tail(qz_state_t* st, qz_obj_t obj)
{
  for(;;) {
    qz_obj_t rest = qz_rest(obj);
    if(qz_is_null(rest))
      return obj;
    if(!qz_is_pair(rest))
      return qz_error(st, "expected list");
    obj = rest;
  }
}

static qz_obj_t qq(qz_state_t*, qz_obj_t, int);

static qz_obj_t qq_or_splice(qz_state_t* st, qz_obj_t in, int depth, int* splice)
{
  *splice = 0;

  /* <qq template or splice D> -> <splicing unquotation D>
   * <splicing unquotation D> -> (unquote-splicing <qq template D - 1>) */
  if(qz_is_pair(in) && qz_eqv(qz_first(in), st->unquote_splicing_sym)) {
    *splice = 1;
    return qq(st, qz_first(qz_rest(in)), depth - 1);
  }

  /* <qq template or splice D> -> <qq template D> */
  return qq(st, in, depth);
}

static intptr_t list_length(qz_obj_t obj);

static qz_obj_t qq_list(qz_state_t* st, qz_obj_t in, int depth)
{
  qz_obj_t obj = qz_first(in);
  in = qz_rest(in);

  /* <unquotation D> -> (unquote <qq template D - 1>) */
  if(qz_eqv(obj, st->unquote_sym))
    return qq(st, qz_first(in), depth - 1);

  /* <list qq template D> -> (quote <qq template D>) */
  if(qz_eqv(obj, st->quote_sym))
    return qq(st, qz_first(in), depth);

  /* <list qq template D> -> (quasiquote <qq template D + 1>) */
  if(qz_eqv(obj, st->quasiquote_sym))
    return qq(st, qz_first(in), depth + 1);

  /* <list qq template D> -> (<qq template or splice D>*)
   *   | (<qq template or splice D>+ . <qq template D>) */
  qz_obj_t result = QZ_NULL;
  qz_obj_t out;

  for(;;)
  {
    int splice;
    obj = qq_or_splice(st, obj, depth, &splice);
    if(!splice)
      obj = qz_make_pair(obj, QZ_NULL);

    if(!(splice && qz_is_null(obj)))
    {
      if(qz_is_null(result)) {
        result = obj;
        qz_push_safety(st, result);
      }
      else {
        qz_to_pair(out)->rest = obj;
      }

      out = qz_is_pair(obj) ? list_tail(st, obj) : obj;
    }

    if(qz_is_null(in))
      break; /* ran out of elements in proper list */

    /* ugly special cases */
    if(!qz_is_pair(in)) {
      qz_to_pair(out)->rest = qq(st, in, depth);
      break; /* ran out of elements in improper list */
    }

    if(qz_eqv(qz_first(in), st->unquote_sym) && list_length(in) == 2) {
      qz_to_pair(out)->rest = qq(st, in, depth);
      break; /* trailing unquote in improper list */
    }

    obj = qz_first(in);
    in = qz_rest(in);
  }

  if(!qz_is_null(result))
    qz_pop_safety(st, 1);

  return result;
}

static qz_obj_t qq_vector(qz_state_t* st, qz_obj_t in, int depth)
{
  /* <vector qq template D> -> #(<qq template or splice D>*) */
  qz_cell_t* in_cell = qz_to_cell(in);
  size_t len = in_cell->value.array.size;

  qz_cell_t* out_cell = qz_make_cell(QZ_CT_VECTOR, len*sizeof(qz_obj_t));
  out_cell->value.array.size = len;
  out_cell->value.array.capacity = len;

  qz_push_safety(st, qz_from_cell(out_cell));

  for(size_t i = 0; i < len; i++)
  {
    qz_obj_t obj = QZ_CELL_DATA(in_cell, qz_obj_t)[i];

    int splice;
    obj = qq_or_splice(st, obj, depth, &splice);

    /* TODO support splicing into vectors */

    QZ_CELL_DATA(out_cell, qz_obj_t)[i] = obj;
  }

  qz_pop_safety(st, 1);
  return qz_from_cell(out_cell);
}

static qz_obj_t qq(qz_state_t* st, qz_obj_t in, int depth)
{
  /* <qq template 0> -> <expression> */
  if(depth == 0)
    return qz_eval(st, in);

  /* <qq template D> -> <list qq template D>
   *   | <unquotation D> */
  if(qz_is_pair(in))
    return qq_list(st, in, depth);

  /* <qq template D> -> <vector qq template D> */
  if(qz_is_vector(in))
    return qq_vector(st, in, depth);

  /* <qq template D> -> <simple datum> */
  return qz_ref(st, in);
}

QZ_DEF_CFUN(scm_quasiquote)
{
  /* <quasiquotation> -> <quasiquotation 1>
   * <quasiquotation D> -> (quasiquote <qq template D>) */
  return qq(st, qz_first(args), 1);
}

/******************************************************************************
 * 5.2. Definitions
 ******************************************************************************/

QZ_DEF_CFUN(scm_define)
{
  qz_obj_t header = qz_required_arg(st, &args);

  if(qz_is_sym(header))
  {
    /* plain variable creation and assignment */
    qz_obj_t expr = qz_required_arg(st, &args);
    qz_obj_t value = qz_eval(st, expr);

    set_var(st, header, value);
  }
  else if(qz_is_pair(header))
  {
    /* function creation and assignment */
    qz_obj_t var = qz_required_arg(st, &header);
    if(!qz_is_sym(var))
      return qz_error(st, "function variant of define not given symbol");

    qz_obj_t formals = qz_ref(st, header);
    qz_obj_t body = qz_make_pair(st->begin_sym, qz_ref(st, args));

    qz_cell_t* cell = qz_make_cell(QZ_CT_FUN, 0);

    cell->value.pair.first = qz_ref(st, qz_list_head(st->env));
    cell->value.pair.rest = qz_make_pair(formals, body);

    set_var(st, var, qz_from_cell(cell));
  }
  else
  {
    return qz_error(st, "first argument to define must be a symbol or list");
  }

  return QZ_NONE;
}

/******************************************************************************
 * 6.1. Equivalence predicates
 ******************************************************************************/

static qz_obj_t compare(qz_state_t* st, qz_obj_t args, cmp_fun cf)
{
  qz_obj_t obj1, obj2;
  qz_get_args(st, &args, "aa", &obj1, &obj2);

  int eq = cf(obj1, obj2);

  qz_unref(st, obj1);
  qz_unref(st, obj2);

  return eq ? QZ_TRUE : QZ_FALSE;
}

QZ_DEF_CFUN(scm_eq_q)
{
  return compare(st, args, qz_eq);
}

QZ_DEF_CFUN(scm_eqv_q)
{
  return compare(st, args, qz_eqv);
}

QZ_DEF_CFUN(scm_equal_q)
{
  return compare(st, args, qz_equal);
}

/******************************************************************************
 * 6.2. Numbers
 ******************************************************************************/

/* 6.2.6. Numerical operations */

static int compare_fixnum(qz_obj_t a, qz_obj_t b)
{
  return a.value - b.value;
}

QZ_DEF_CFUN(scm_num_eq)
{
  return compare_many(st, args, qz_is_fixnum, compare_fixnum, EQUAL);
}

QZ_DEF_CFUN(scm_num_lt)
{
  return compare_many(st, args, qz_is_fixnum, compare_fixnum, LESS);
}

QZ_DEF_CFUN(scm_num_gt)
{
  return compare_many(st, args, qz_is_fixnum, compare_fixnum, GREATER);
}

QZ_DEF_CFUN(scm_num_lte)
{
  return compare_many(st, args, qz_is_fixnum, compare_fixnum, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_num_gte)
{
  return compare_many(st, args, qz_is_fixnum, compare_fixnum, GREATER|EQUAL);
}

QZ_DEF_CFUN(scm_num_add)
{
  qz_obj_t result = qz_from_fixnum(0);

  for(;;)
  {
    qz_obj_t expr = qz_optional_arg(st, &args);
    if(qz_is_none(expr))
      return result;

    qz_obj_t value = qz_eval(st, expr);
    if(!qz_is_fixnum(value)) {
      qz_unref(st, value);
      return qz_error(st, "expected fixnum");
    }

    result = qz_from_fixnum(qz_to_fixnum(result) + qz_to_fixnum(value));
  }
}

QZ_DEF_CFUN(scm_num_mul)
{
  qz_obj_t result = qz_from_fixnum(1);

  for(;;)
  {
    qz_obj_t expr = qz_optional_arg(st, &args);
    if(qz_is_none(expr))
      return result;

    qz_obj_t value = qz_eval(st, expr);
    if(!qz_is_fixnum(value)) {
      qz_unref(st, value);
      return qz_error(st, "expected fixnum");
    }

    result = qz_from_fixnum(qz_to_fixnum(result) * qz_to_fixnum(value));
  }
}

QZ_DEF_CFUN(scm_num_sub)
{
  qz_obj_t expr = qz_required_arg(st, &args);

  qz_obj_t result = qz_eval(st, expr);
  if(!qz_is_fixnum(result)) {
    qz_unref(st, result);
    return qz_error(st, "expected fixnum");
  }

  expr = qz_optional_arg(st, &args);
  if(qz_is_none(expr))
    return qz_from_fixnum(-qz_to_fixnum(result)); /* single argument form negates */

  for(;;)
  {
    qz_obj_t value = qz_eval(st, expr);
    if(!qz_is_fixnum(value)) {
      qz_unref(st, value);
      qz_error(st, "expected fixnum");
    }

    result = qz_from_fixnum(qz_to_fixnum(result) - qz_to_fixnum(value));

    expr = qz_optional_arg(st, &args);
    if(qz_is_none(expr))
      return result; /* ran out of expressions */
  }
}

/******************************************************************************
 * 6.3. Booleans
 ******************************************************************************/

QZ_DEF_CFUN(scm_not)
{
  qz_obj_t value;
  qz_get_args(st, &args, "a", &value);

  if(qz_eq(value, QZ_FALSE))
    return QZ_TRUE;

  qz_unref(st, value);
  return QZ_FALSE;
}

QZ_DEF_CFUN(scm_boolean_q)
{
  return predicate(st, args, qz_is_bool);
}

/******************************************************************************
 * 6.4. Pairs and lists
 ******************************************************************************/

QZ_DEF_CFUN(scm_pair_q)
{
  return predicate(st, args, qz_is_pair);
}

QZ_DEF_CFUN(scm_cons)
{
  qz_obj_t obj1, obj2;
  qz_get_args(st, &args, "aa", &obj1, &obj2);

  return qz_make_pair(obj1, obj2);
}

QZ_DEF_CFUN(scm_car)
{
  qz_obj_t pair;
  qz_get_args(st, &args, "p", &pair);

  qz_obj_t first = qz_ref(st, qz_first(pair));
  qz_unref(st, pair);
  return first;
}

QZ_DEF_CFUN(scm_cdr)
{
  qz_obj_t pair;
  qz_get_args(st, &args, "p", &pair);

  qz_obj_t rest = qz_ref(st, qz_rest(pair));
  qz_unref(st, pair);
  return rest;
}

QZ_DEF_CFUN(scm_set_car_b)
{
  qz_obj_t pair, first;
  qz_get_args(st, &args, "pa", &pair, &first);

  qz_pair_t* pair_raw = qz_to_pair(pair);
  qz_unref(st, pair_raw->first);
  pair_raw->first = first;

  qz_unref(st, pair);
  return QZ_NONE;
}

QZ_DEF_CFUN(scm_set_cdr_b)
{
  qz_obj_t pair, rest;
  qz_get_args(st, &args, "pa", &pair, &rest);

  qz_pair_t* pair_raw = qz_to_pair(pair);
  qz_unref(st, pair_raw->rest);
  pair_raw->rest = rest;

  qz_unref(st, pair);
  return QZ_NONE;
}

QZ_DEF_CFUN(scm_null_q)
{
  return predicate(st, args, qz_is_null);
}

static int is_list(qz_obj_t obj)
{
  for(;;) {
    if(qz_is_null(obj))
      return 1;

    if(!qz_is_pair(obj))
      return 0;

    obj = qz_rest(obj);
  }
}

QZ_DEF_CFUN(scm_list_q)
{
  return predicate(st, args, is_list);
}

QZ_DEF_CFUN(scm_make_list)
{
  qz_obj_t k, fill;
  qz_get_args(st, &args, "ia?", &k, &fill);

  qz_obj_t result = QZ_NULL;

  for(intptr_t i = qz_to_fixnum(k); i > 0; i--)
    result = qz_make_pair(qz_ref(st, fill), result);

  qz_unref(st, fill);

  return result;
}

QZ_DEF_CFUN(scm_list)
{
  qz_obj_t result = QZ_NULL;
  qz_obj_t elem = QZ_NULL;

  for(;;) {
    qz_obj_t obj;
    qz_get_args(st, &args, "a?", &obj);
    if(qz_is_none(obj))
      return result;

    qz_obj_t inner_elem = qz_make_pair(obj, QZ_NULL);

    if(qz_is_null(elem)) {
      result = elem = inner_elem;
    }
    else {
      qz_to_pair(elem)->rest = inner_elem;
      elem = inner_elem;
    }
  }
}

static intptr_t list_length(qz_obj_t obj)
{
  intptr_t len = 0;
  for(;;) {
    if(qz_is_null(obj))
      return len;

    if(!qz_is_pair(obj))
      return -1;

    len++;
    obj = qz_rest(obj);
  }
}

QZ_DEF_CFUN(scm_length)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "a", &obj);

  intptr_t len = list_length(obj);

  qz_unref(st, obj);

  if(len < 0)
    return qz_error(st, "expected list");

  return qz_from_fixnum(len);
}

/* TODO append */

QZ_DEF_CFUN(scm_reverse)
{
  qz_obj_t list = qz_eval(st, qz_required_arg(st, &args));
  qz_obj_t result = QZ_NULL;

  qz_obj_t elem = list;
  for(;;) {
    if(qz_is_null(elem)) {
      qz_unref(st, list);
      return result;
    }

    if(!qz_is_pair(elem)) {
      qz_unref(st, list);
      qz_unref(st, result);
      return qz_error(st, "expected list");
    }

    result = qz_make_pair(qz_first(elem), result);
    elem = qz_rest(elem);
  }
}

QZ_DEF_CFUN(scm_list_tail)
{
  qz_obj_t list, k;
  qz_get_args(st, &args, "pi", &list, &k);

  qz_obj_t elem = list;
  for(intptr_t i = qz_to_fixnum(k); i > 0; i--)
  {
    if(qz_is_null(elem)) {
      qz_unref(st, list);
      return qz_error(st, "list too short");
    }

    if(!qz_is_pair(elem)) {
      qz_unref(st, list);
      return qz_error(st, "expected list");
    }

    elem = qz_rest(elem);
  }

  qz_ref(st, elem);
  qz_unref(st, list);
  return elem;
}

QZ_DEF_CFUN(scm_list_ref)
{
  return qz_first(scm_list_tail(st, args));
}

QZ_DEF_CFUN(scm_list_set_b)
{
  qz_obj_t elem = scm_list_tail(st, args);

  qz_push_safety(st, elem);

  if(!qz_is_pair(elem))
    return qz_error(st, "expected list");

  qz_obj_t obj = qz_eval(st, qz_required_arg(st, &args));

  qz_pop_safety(st, 1);

  qz_pair_t* pair = qz_to_pair(elem);
  qz_unref(st, pair->first);
  pair->first = obj;

  return QZ_NONE;
}

static qz_obj_t inner_member(qz_state_t* st, qz_obj_t args, cmp_fun cf)
{
  qz_obj_t obj, list;
  qz_get_args(st, &args, "ap", &obj, &list);

  qz_obj_t custom_cmp = QZ_NONE;
  if(cf == qz_equal) {
    custom_cmp = qz_optional_arg(st, &args);
    if(!qz_is_none(custom_cmp)) {
      qz_obj_t args = qz_make_pair(QZ_NULL, qz_make_pair(qz_ref(st, obj), QZ_NULL));
      custom_cmp = qz_make_pair(qz_eval(st, custom_cmp), args);
      qz_push_safety(st, custom_cmp);
    }
  }

  qz_obj_t elem = list;
  qz_obj_t result = QZ_FALSE;
  for(;;) {
    if(qz_is_null(result))
      break;

    if(!qz_is_pair(elem))
      return qz_error(st, "expected list");

    int match;
    if(!qz_is_none(custom_cmp)) {
      qz_obj_t* arg = &qz_to_pair(qz_rest(custom_cmp))->first;
      /* insert element into args */
      qz_unref(st, *arg);
      *arg = qz_ref(st, qz_first(elem));
      /* call custom comparator */
      qz_obj_t custom_cmp_result = qz_eval(st, custom_cmp);
      /* check for equality */
      match = !qz_eq(custom_cmp_result, QZ_FALSE);
      /* cleanup */
      qz_unref(st, custom_cmp_result);
    }
    else {
      match = cf(qz_first(elem), obj);
    }

    if(match) {
      result = qz_ref(st, elem);
      break;
    }

    elem = qz_rest(elem);
  }

  if(!qz_is_none(custom_cmp)) {
    qz_pop_safety(st, 1);
    qz_unref(st, custom_cmp);
  }
  qz_pop_safety(st, 2);
  qz_unref(st, obj);
  qz_unref(st, list);

  return result;
}

QZ_DEF_CFUN(scm_memq)
{
  return inner_member(st, args, qz_eq);
}

QZ_DEF_CFUN(scm_memv)
{
  return inner_member(st, args, qz_eqv);
}

QZ_DEF_CFUN(scm_member)
{
  return inner_member(st, args, qz_equal);
}

static qz_obj_t inner_assoc(qz_state_t* st, qz_obj_t args, cmp_fun cf)
{
  qz_obj_t obj = inner_member(st, args, cf);

  if(qz_eq(obj, QZ_FALSE))
    return QZ_FALSE;

  qz_obj_t result = qz_ref(st, qz_first(obj));
  qz_unref(st, obj);

  return result;
}

QZ_DEF_CFUN(scm_assq)
{
  return inner_assoc(st, args, qz_eq);
}

QZ_DEF_CFUN(scm_assv)
{
  return inner_assoc(st, args, qz_eqv);
}

QZ_DEF_CFUN(scm_assoc)
{
  return inner_assoc(st, args, qz_equal);
}

QZ_DEF_CFUN(scm_list_copy)
{
  qz_obj_t list = qz_eval(st, qz_required_arg(st, &args));
  qz_push_safety(st, list);

  qz_obj_t elem = list;
  qz_obj_t result = QZ_NULL;

  for(;;) {
    if(qz_is_null(elem)) {
      qz_pop_safety(st, 1);
      qz_unref(st, list);
      return result; /* ran out of elements */
    }

    if(!qz_is_pair(elem))
      return qz_error(st, "expected list");

    qz_obj_t inner_result = qz_make_pair(qz_ref(st, qz_first(elem)), QZ_NULL);

    if(!qz_is_null(result))
      qz_to_pair(result)->rest = inner_result;

    result = inner_result;
    elem = qz_rest(elem);
  }
}

/******************************************************************************
 * 6.5. Symbols
 ******************************************************************************/

QZ_DEF_CFUN(scm_symbol_q)
{
  return predicate(st, args, qz_is_sym);
}

QZ_DEF_CFUN(scm_symbol_a_string)
{
  qz_obj_t sym;
  qz_get_args(st, &args, "n", &sym);

  qz_obj_t* str = qz_hash_get(st, st->sym_name, sym);
  assert(str && qz_is_string(*str));

  return qz_ref(st, *str);
}

QZ_DEF_CFUN(scm_string_a_symbol)
{
  qz_obj_t str;
  qz_get_args(st, &args, "s", &str);

  return qz_make_sym(st, str);
}

/******************************************************************************
 * 6.6. Characters
 ******************************************************************************/

static int compare_char(qz_obj_t a, qz_obj_t b)
{
  return a.value - b.value;
}

static int compare_char_ci(qz_obj_t a, qz_obj_t b)
{
  return tolower(qz_to_char(a)) - tolower(qz_to_char(b));
}

QZ_DEF_CFUN(scm_char_q)
{
  return predicate(st, args, qz_is_char);
}

QZ_DEF_CFUN(scm_char_eq_q)
{
  return compare_many(st, args, qz_is_char, compare_char, EQUAL);
}

QZ_DEF_CFUN(scm_char_lt_q)
{
  return compare_many(st, args, qz_is_char, compare_char, LESS);
}

QZ_DEF_CFUN(scm_char_gt_q)
{
  return compare_many(st, args, qz_is_char, compare_char, GREATER);
}

QZ_DEF_CFUN(scm_char_lte_q)
{
  return compare_many(st, args, qz_is_char, compare_char, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_char_gte_q)
{
  return compare_many(st, args, qz_is_char, compare_char, GREATER|EQUAL);
}

QZ_DEF_CFUN(scm_char_ci_eq_q)
{
  return compare_many(st, args, qz_is_char, compare_char_ci, EQUAL);
}

QZ_DEF_CFUN(scm_char_ci_lt_q)
{
  return compare_many(st, args, qz_is_char, compare_char_ci, LESS);
}

QZ_DEF_CFUN(scm_char_ci_gt_q)
{
  return compare_many(st, args, qz_is_char, compare_char_ci, GREATER);
}

QZ_DEF_CFUN(scm_char_ci_lte_q)
{
  return compare_many(st, args, qz_is_char, compare_char_ci, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_char_ci_gte_q)
{
  return compare_many(st, args, qz_is_char, compare_char_ci, GREATER|EQUAL);
}

typedef int (*char_pred_fun)(int);

static qz_obj_t inner_char_predicate(qz_state_t* st, qz_obj_t args, char_pred_fun cpf)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));
  if(!qz_is_char(value)) {
    qz_unref(st, value);
    return QZ_FALSE;
  }
  return cpf(qz_to_char(value)) ? QZ_TRUE : QZ_FALSE;
}

QZ_DEF_CFUN(scm_char_alphabetic_q)
{
  return inner_char_predicate(st, args, isalpha);
}

QZ_DEF_CFUN(scm_char_numeric_q)
{
  return inner_char_predicate(st, args, isdigit);
}

QZ_DEF_CFUN(scm_char_whitespace_q)
{
  return inner_char_predicate(st, args, isspace);
}

QZ_DEF_CFUN(scm_char_upper_case_q)
{
  return inner_char_predicate(st, args, isupper);
}

QZ_DEF_CFUN(scm_char_lower_case_q)
{
  return inner_char_predicate(st, args, islower);
}

QZ_DEF_CFUN(scm_digit_value)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));
  if(!qz_is_char(value)) {
    qz_unref(st, value);
    return QZ_FALSE;
  }
  char ch = qz_to_char(value);
  return isdigit(ch) ? qz_from_fixnum(ch - '0') : QZ_FALSE;
}

QZ_DEF_CFUN(scm_char_a_integer)
{
  qz_obj_t value;
  qz_get_args(st, &args, "c", &value);
  return qz_from_fixnum(qz_to_char(value));
}

QZ_DEF_CFUN(scm_integer_a_char)
{
  qz_obj_t value;
  qz_get_args(st, &args, "i", &value);
  return qz_from_char(qz_to_fixnum(value));
}

/******************************************************************************
 * 6.7. Strings
 ******************************************************************************/

QZ_DEF_CFUN(scm_string_q)
{
  return predicate(st, args, qz_is_string);
}

QZ_DEF_CFUN(scm_make_string)
{
  qz_obj_t k, fill;
  qz_get_args(st, &args, "ic?", &k, &fill);

  intptr_t k_raw = qz_to_fixnum(k);
  if(k_raw < 0)
    return qz_error(st, "bad string length");

  qz_cell_t* cell = qz_make_cell(QZ_CT_STRING, k_raw*sizeof(char));
  cell->value.array.size = k_raw;
  cell->value.array.capacity = k_raw;

  if(!qz_is_none(fill))
    memset(QZ_CELL_DATA(cell, char), qz_to_char(fill), k_raw*sizeof(char));

  return qz_from_cell(cell);
}

/* TODO string */

static qz_obj_t array_length(qz_state_t* st, qz_obj_t args, const char* type_char)
{
  qz_obj_t obj;
  qz_get_args(st, &args, type_char, &obj);
  size_t len = qz_to_cell(obj)->value.array.size;
  qz_unref(st, obj);
  return qz_from_fixnum(len);
}

QZ_DEF_CFUN(scm_string_length)
{
  return array_length(st, args, "s");
}


static qz_obj_t string_ref(qz_state_t* st, qz_cell_t* cell, size_t i)
{
  return qz_from_char(QZ_CELL_DATA(cell, char)[i]);
}

QZ_DEF_CFUN(scm_string_ref)
{
  return array_ref(st, args, "si", string_ref);
}

static void string_set(qz_state_t* st, qz_cell_t* cell, size_t i, qz_obj_t obj)
{
  QZ_CELL_DATA(cell, char)[i] = qz_to_char(obj);
}

QZ_DEF_CFUN(scm_string_set_b)
{
  return array_set(st, args, "sic", string_set);
}

static int min(size_t a, size_t b)
{
  return a < b ? a : b;
}

static int memicmp(const void* s1, const void* s2, size_t n)
{
  for(size_t i = 0; i < n; i++) {
    int d = tolower(((const char*)s1)[i]) - tolower(((const char*)s2)[i]);
    if(d) return d;
  }
  return 0;
}

typedef int (*memcmp_fun)(const void*, const void*, size_t);

static int compare_string(qz_obj_t a, qz_obj_t b, memcmp_fun mcf)
{
  qz_cell_t* a_cell = qz_to_cell(a);
  qz_cell_t* b_cell = qz_to_cell(b);

  int cmp = mcf(QZ_CELL_DATA(a_cell, char), QZ_CELL_DATA(b_cell, char),
                min(a_cell->value.array.size, b_cell->value.array.size));

  if(cmp == 0)
    return a_cell->value.array.size - b_cell->value.array.size;

  return cmp;
}

static int compare_string_cs(qz_obj_t a, qz_obj_t b)
{
  return compare_string(a, b, memcmp);
}

static int compare_string_ci(qz_obj_t a, qz_obj_t b)
{
  return compare_string(a, b, memicmp);
}

/* TODO -ni variants */

QZ_DEF_CFUN(scm_string_eq_q)
{
  return compare_many(st, args, qz_is_string, compare_string_cs, EQUAL);
}

QZ_DEF_CFUN(scm_string_ci_eq_q)
{
  return compare_many(st, args, qz_is_string, compare_string_ci, EQUAL);
}

QZ_DEF_CFUN(scm_string_lt_q)
{
  return compare_many(st, args, qz_is_string, compare_string_cs, LESS);
}

QZ_DEF_CFUN(scm_string_ci_lt_q)
{
  return compare_many(st, args, qz_is_string, compare_string_ci, LESS);
}

QZ_DEF_CFUN(scm_string_gt_q)
{
  return compare_many(st, args, qz_is_string, compare_string_cs, GREATER);
}

QZ_DEF_CFUN(scm_string_ci_gt_q)
{
  return compare_many(st, args, qz_is_string, compare_string_ci, GREATER);
}

QZ_DEF_CFUN(scm_string_lte_q)
{
  return compare_many(st, args, qz_is_string, compare_string_cs, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_string_ci_lte_q)
{
  return compare_many(st, args, qz_is_string, compare_string_ci, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_string_gte_q)
{
  return compare_many(st, args, qz_is_string, compare_string_cs, GREATER|EQUAL);
}

QZ_DEF_CFUN(scm_string_ci_gte_q)
{
  return compare_many(st, args, qz_is_string, compare_string_ci, GREATER|EQUAL);
}

typedef int (*xformchar_fun)(int);

static qz_obj_t transform_string(qz_state_t* st, qz_obj_t args, xformchar_fun xcf)
{
  qz_obj_t str;
  qz_get_args(st, &args, "s", &str);

  qz_cell_t* in = qz_to_cell(str);
  size_t len = in->value.array.size;

  qz_cell_t* out = qz_make_cell(QZ_CT_STRING, len*sizeof(char));
  out->value.array.size = len;
  out->value.array.capacity = len;

  for(size_t i = 0; i < len; i++)
    QZ_CELL_DATA(out, char)[i] = xcf(QZ_CELL_DATA(in, char)[i]);

  qz_unref(st, str);

  return qz_from_cell(out);
}

QZ_DEF_CFUN(scm_string_upcase)
{
  return transform_string(st, args, toupper);
}

QZ_DEF_CFUN(scm_string_downcase)
{
  return transform_string(st, args, tolower);
}

/* TODO string-foldcase */

QZ_DEF_CFUN(scm_substring)
{
  qz_obj_t str, start, end;
  qz_get_args(st, &args, "sii", &str, &start, &end);

  qz_cell_t* in = qz_to_cell(str);
  intptr_t start_raw = qz_to_fixnum(start);
  intptr_t end_raw = qz_to_fixnum(end);

  if(start_raw < 0 || end_raw < start_raw || in->value.array.size < end_raw) {
    qz_unref(st, str);
    return qz_error(st, "index out of bounds");
  }

  qz_cell_t* out = qz_make_cell(QZ_CT_STRING, (end_raw-start_raw)*sizeof(char));

  memcpy(QZ_CELL_DATA(out, char),
         QZ_CELL_DATA(in, char) + start_raw,
         (end_raw-start_raw)*sizeof(char));

  qz_unref(st, str);

  return qz_from_cell(out);
}

/* TODO string->list */

/* TODO list->string */

static int identity(int i)
{
  return i;
}

QZ_DEF_CFUN(scm_string_copy)
{
  return transform_string(st, args, identity);
}

/* TODO string-fill! */

/******************************************************************************
 * 6.8. Vectors
 ******************************************************************************/

QZ_DEF_CFUN(scm_vector_q)
{
  return predicate(st, args, qz_is_vector);
}

QZ_DEF_CFUN(scm_make_vector)
{
  qz_obj_t k, fill;
  qz_get_args(st, &args, "ia?", &k, &fill);

  intptr_t k_raw = qz_to_fixnum(k);
  if(k_raw < 0) {
    qz_unref(st, fill);
    return qz_error(st, "bad vector length");
  }

  qz_cell_t* cell = qz_make_cell(QZ_CT_STRING, k_raw*sizeof(qz_obj_t));
  cell->value.array.size = k_raw;
  cell->value.array.capacity = k_raw;

  if(!qz_is_none(fill)) {
    for(size_t i = 0; i < k_raw; i++)
      QZ_CELL_DATA(cell, qz_obj_t)[i] = qz_ref(st, fill);
    qz_unref(st, fill);
  }

  return qz_from_cell(cell);
}

QZ_DEF_CFUN(scm_vector_length)
{
  return array_length(st, args, "v");
}

static qz_obj_t vector_ref(qz_state_t* st, qz_cell_t* cell, size_t i)
{
  return qz_ref(st, QZ_CELL_DATA(cell, qz_obj_t)[i]);
}

QZ_DEF_CFUN(scm_vector_ref)
{
  return array_ref(st, args, "vi", vector_ref);
}

static void vector_set(qz_state_t* st, qz_cell_t* cell, size_t i, qz_obj_t obj)
{
  qz_obj_t* slot = QZ_CELL_DATA(cell, qz_obj_t) + i;
  qz_unref(st, *slot);
  *slot = obj;
}

QZ_DEF_CFUN(scm_vector_set_b)
{
  return array_set(st, args, "via", vector_set);
}

QZ_DEF_CFUN(scm_vector_a_list)
{
  qz_obj_t vec;
  qz_get_args(st, &args, "v", &vec);

  qz_cell_t* cell = qz_to_cell(vec);
  if(cell->value.array.size == 0)
    return QZ_NULL;

  qz_obj_t result = QZ_NULL;
  qz_obj_t elem = QZ_NULL;

  for(size_t i = 0; i < cell->value.array.size; i++)
  {
    qz_obj_t inner_elem = qz_make_pair(qz_ref(st, QZ_CELL_DATA(cell, qz_obj_t)[i]), QZ_NULL);

    if(qz_is_null(elem)) {
      result = elem = inner_elem;
    }
    else {
      qz_to_pair(elem)->rest = inner_elem;
      elem = inner_elem;
    }
  }

  qz_unref(st, vec);
  return result;
}

QZ_DEF_CFUN(scm_list_a_vector)
{
  qz_obj_t list;
  qz_get_args(st, &args, "a", &list);

  intptr_t len = list_length(list);

  if(len < 0) {
    qz_unref(st, list);
    return qz_error(st, "expected list");
  }

  qz_cell_t* cell = qz_make_cell(QZ_CT_VECTOR, len*sizeof(qz_obj_t));

  qz_obj_t elem = list;
  for(size_t i = 0; i < len; i++) {
    QZ_CELL_DATA(cell, qz_obj_t)[i] = qz_ref(st, qz_first(elem));
    elem = qz_rest(elem);
  }

  qz_unref(st, list);

  return qz_from_cell(cell);
}

QZ_DEF_CFUN(scm_vector_a_string)
{
  qz_obj_t vec;
  qz_get_args(st, &args, "v", &vec);

  qz_cell_t* in = qz_to_cell(vec);
  size_t len = in->value.array.size;

  qz_cell_t* out = qz_make_cell(QZ_CT_STRING, len*sizeof(char));
  out->value.array.size = len;
  out->value.array.capacity = len;

  for(size_t i = 0; i < len; i++) {
    qz_obj_t ch = QZ_CELL_DATA(in, qz_obj_t)[i];
    if(!qz_is_char(ch)) {
      qz_unref(st, vec);
      qz_unref(st, qz_from_cell(out));
      return qz_error(st, "expected char");
    }
    QZ_CELL_DATA(out, char)[i] = qz_to_char(ch);
  }

  qz_unref(st, vec);
  return qz_from_cell(out);
}

QZ_DEF_CFUN(scm_string_a_vector)
{
  qz_obj_t str;
  qz_get_args(st, &args, "s", &str);

  qz_cell_t* in = qz_to_cell(str);
  size_t len = in->value.array.size;

  qz_cell_t* out = qz_make_cell(QZ_CT_VECTOR, len*sizeof(qz_obj_t));
  out->value.array.size = len;
  out->value.array.capacity = len;

  for(size_t i = 0; i < len; i++)
    QZ_CELL_DATA(out, qz_obj_t)[i] = qz_from_char(QZ_CELL_DATA(in, char)[i]);

  qz_unref(st, str);
  return qz_from_cell(out);
}

QZ_DEF_CFUN(scm_vector_copy)
{
  qz_obj_t vec, start, end, fill;
  qz_get_args(st, &args, "vi?i?a?", &vec, &start, &end, &fill);

  qz_cell_t* in = qz_to_cell(vec);
  size_t in_len = in->value.array.size;

  intptr_t start_raw = qz_is_none(start) ? 0 : qz_to_fixnum(start);
  intptr_t end_raw = qz_is_none(end) ? in_len : qz_to_fixnum(end);

  if(start_raw > end_raw) {
    qz_unref(st, vec);
    return qz_error(st, "invalid index");
  }

  size_t out_len = end_raw - start_raw;
  qz_cell_t* out = qz_make_cell(QZ_CT_VECTOR, out_len*sizeof(qz_obj_t));
  out->value.array.size = out_len;
  out->value.array.capacity = out_len;

  for(size_t i = 0; i < out_len; i++)
  {
    if((i + start_raw < 0) || (i + start_raw >= in_len)) {
      QZ_CELL_DATA(out, qz_obj_t)[i] = QZ_NONE;
    }
    else {
      QZ_CELL_DATA(out, qz_obj_t)[i] = qz_ref(st, QZ_CELL_DATA(in, qz_obj_t)[i + start_raw]);
    }
  }

  qz_unref(st, vec);
  return qz_from_cell(out);
}

/* TODO vector-fill! */

/******************************************************************************
 * 6.9. Bytevectors
 ******************************************************************************/

QZ_DEF_CFUN(scm_bytevector_q)
{
  return predicate(st, args, qz_is_bytevector);
}

QZ_DEF_CFUN(scm_make_bytevector)
{
  qz_obj_t k, fill;
  qz_get_args(st, &args, "ii?", &k, &fill);

  intptr_t k_raw = qz_to_fixnum(k);
  if(k_raw < 0)
    return qz_error(st, "bad bytevector length");

  qz_cell_t* cell = qz_make_cell(QZ_CT_BYTEVECTOR, k_raw*sizeof(uint8_t));
  cell->value.array.size = k_raw;
  cell->value.array.capacity = k_raw;

  if(!qz_is_none(fill))
    memset(QZ_CELL_DATA(cell, uint8_t), qz_to_fixnum(fill), k_raw*sizeof(uint8_t));

  return qz_from_cell(cell);
}

QZ_DEF_CFUN(scm_bytevector_length)
{
  return array_length(st, args, "w");
}

static qz_obj_t bytevector_ref(qz_state_t* st, qz_cell_t* cell, size_t i)
{
  return qz_from_fixnum(QZ_CELL_DATA(cell, uint8_t)[i]);
}

QZ_DEF_CFUN(scm_bytevector_u8_ref)
{
  return array_ref(st, args, "wi", bytevector_ref);
}

static void bytevector_set(qz_state_t* st, qz_cell_t* cell, size_t i, qz_obj_t obj)
{
  QZ_CELL_DATA(cell, uint8_t)[i] = qz_to_fixnum(obj);
}

QZ_DEF_CFUN(scm_bytevector_u8_set_b)
{
  return array_set(st, args, "wii", bytevector_set);
}

/* TODO bytevector-copy-partial */

/* TODO bytevector-copy-partial! */

/******************************************************************************
 * 6.10. Control features
 ******************************************************************************/

static int is_procedure(qz_obj_t obj)
{
  return qz_is_cfun(obj) || qz_is_fun(obj);
}

QZ_DEF_CFUN(scm_procedure_q)
{
  return predicate(st, args, is_procedure);
}

/* TODO apply */

/******************************************************************************
 * 6.11. Exceptions
 ******************************************************************************/

/* this is the default error handler
 * it is also used as a failsafe when executing a user-supplied error handler
 * it is guaranteed to not throw an exception so we don't get into loop */
ALIGNED qz_obj_t qz_error_handler(qz_state_t* st, qz_obj_t args)
{
  qz_obj_t obj = qz_optional_arg(st, &args);

  fputs("An error was caught: ", stderr);
  qz_write(st, obj, -1, stderr);
  fputc('\n', stderr);

  return QZ_NONE;
}

QZ_DEF_CFUN(scm_with_exception_handler)
{
  qz_obj_t handler, thunk;
  qz_get_args(st, &args, "aa", &handler, &thunk);

  thunk = qz_make_pair(thunk, QZ_NULL);

  /* push error handler */
  qz_obj_t old_handler = st->error_handler;
  st->error_handler = handler;

  /* eval thunk */
  qz_obj_t result = qz_peval(st, thunk);

  /* pop error handler */
  st->error_handler = old_handler;

  /* cleanup */
  qz_unref(st, handler);
  qz_unref(st, thunk);

  return result;
}

QZ_DEF_CFUN(scm_raise)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "a", &obj);

  st->error_obj = obj;
  longjmp(*st->peval_fail, 1);

  return QZ_NONE;
}

/******************************************************************************
 * 6.13. Input and output
 ******************************************************************************/

QZ_DEF_CFUN(scm_write)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "a", &obj);

  qz_write(st, obj, -1, stdout);
  qz_unref(st, obj);

  return QZ_NONE;
}

/* 6.13.4. System interface */

QZ_DEF_CFUN(scm_file_exists_q)
{
  qz_obj_t filename;
  qz_get_args(st, &args, "s", &filename);

  struct stat stat_buf;
  int ok = stat(QZ_CELL_DATA(qz_to_cell(filename), char), &stat_buf);

  qz_unref(st, filename);

  if(ok < 0 && errno != ENOENT)
    return qz_error(st, strerror(errno));

  return (ok == 0) ? QZ_TRUE : QZ_FALSE;
}

QZ_DEF_CFUN(scm_delete_file)
{
  qz_obj_t filename;
  qz_get_args(st, &args, "s", &filename);

  int ok = unlink(QZ_CELL_DATA(qz_to_cell(filename), char));

  qz_unref(st, filename);

  if(ok < 0)
    return qz_error(st, strerror(errno));

  return QZ_NONE;
}

extern int g_argc;
extern char** g_argv;

QZ_DEF_CFUN(scm_command_line)
{
  qz_obj_t result = QZ_NULL;
  qz_obj_t elem;

  for(int i = 0; i < g_argc; i++) {
    qz_obj_t inner_elem = qz_make_pair(qz_make_string(g_argv[i]), QZ_NULL);
    if(qz_is_null(result)) {
      result = elem = inner_elem;
    }
    else {
      qz_to_pair(elem)->rest = inner_elem;
      elem = inner_elem;
    }
  }

  return result;
}

QZ_DEF_CFUN(scm_exit)
{
  qz_obj_t obj;
  qz_get_args(st, &args, "a?", &obj);

  int code;

  if(qz_is_none(obj) || qz_eqv(obj, QZ_TRUE)) {
    code = EXIT_SUCCESS;
  }
  else if(qz_eqv(obj, QZ_FALSE)) {
    code = EXIT_FAILURE;
  }
  else if(qz_is_fixnum(obj)) {
    code = qz_to_fixnum(obj);
  }
  else {
    qz_unref(st, obj);
    return qz_error(st, "Could not convert object to exit code");
  }

  exit(code);
  return QZ_NONE;
}

QZ_DEF_CFUN(scm_get_environment_variable)
{
  qz_obj_t name;
  qz_get_args(st, &args, "s", &name);

  const char* value = getenv(QZ_CELL_DATA(qz_to_cell(name), char));
  if(!value)
    return QZ_FALSE;

  return qz_make_string(value);
}

QZ_DEF_CFUN(scm_get_environment_variables)
{
  qz_obj_t result = QZ_NULL;
  qz_obj_t elem;

  for(char** e = __environ; *e; e++) {
    char* sep = strchr(*e, '=');
    if(!sep)
      continue;

    qz_obj_t key = qz_make_string_with_size(*e, sep - *e);
    qz_obj_t value = qz_make_string_with_size(sep + 1, strlen(sep + 1));

    qz_obj_t inner_elem = qz_make_pair(qz_make_pair(key, value), QZ_NULL);
    if(qz_is_null(result)) {
      result = elem = inner_elem;
    }
    else {
      qz_to_pair(elem)->rest = inner_elem;
      elem = inner_elem;
    }
  }

  return result;
}

QZ_DEF_CFUN(scm_current_second)
{
  return qz_from_fixnum(time(NULL));
}

const qz_named_cfun_t QZ_LIB_FUNCTIONS[] = {
  {scm_quote, "quote"},
  {scm_lambda, "lambda"},
  {scm_if, "if"},
  {scm_set_b, "set!"},
  {scm_cond, "cond"},
  {scm_case, "case"},
  {scm_and, "and"},
  {scm_or, "or"},
  {scm_when, "when"},
  {scm_unless, "unless"},
  {scm_let, "let"},
  {scm_let_s, "let*"},
  {scm_begin, "begin"},
  {scm_delay, "delay"},
  {scm_lazy, "lazy"},
  {scm_force, "force"},
  {scm_eager, "eager"},
  {scm_quasiquote, "quasiquote"},
  {scm_define, "define"},
  {scm_eq_q, "eq?"},
  {scm_eqv_q, "eqv?"},
  {scm_equal_q, "equal?"},
  {scm_num_eq, "="},
  {scm_num_lt, "<"},
  {scm_num_gt, ">"},
  {scm_num_lte, "<="},
  {scm_num_gte, ">="},
  {scm_num_add, "+"},
  {scm_num_mul, "*"},
  {scm_num_sub, "-"},
  {scm_not, "not"},
  {scm_boolean_q, "boolean?"},
  {scm_pair_q, "pair?"},
  {scm_cons, "cons"},
  {scm_car, "car"},
  {scm_cdr, "cdr"},
  {scm_set_car_b, "set-car!"},
  {scm_set_cdr_b, "set-cdr!"},
  {scm_null_q, "null?"},
  {scm_list_q, "list?"},
  {scm_make_list, "make-list"},
  {scm_list, "list"},
  {scm_length, "length"},
  {scm_reverse, "reverse"},
  {scm_list_tail, "list-tail"},
  {scm_list_ref, "list-ref"},
  {scm_list_set_b, "list-set!"},
  {scm_memq, "memq"},
  {scm_memv, "memv"},
  {scm_member, "member"},
  {scm_assq, "assq"},
  {scm_assv, "assv"},
  {scm_assoc, "assoc"},
  {scm_list_copy, "list-copy"},
  {scm_symbol_q, "symbol?"},
  {scm_symbol_a_string, "symbol->string"},
  {scm_string_a_symbol, "string->symbol"},
  {scm_char_q, "char?"},
  {scm_char_eq_q, "char=?"},
  {scm_char_lt_q, "char<?"},
  {scm_char_gt_q, "char>?"},
  {scm_char_lte_q, "char<=?"},
  {scm_char_gte_q, "char>=?"},
  {scm_char_ci_eq_q, "char-ci=?"},
  {scm_char_ci_lt_q, "char-ci<?"},
  {scm_char_ci_gt_q, "char-ci>?"},
  {scm_char_ci_lte_q, "char-ci<=?"},
  {scm_char_ci_gte_q, "char-ci>=?"},
  {scm_char_alphabetic_q, "char-alphabetic?"},
  {scm_char_numeric_q, "char-numeric?"},
  {scm_char_whitespace_q, "char-whitespace?"},
  {scm_char_upper_case_q, "char-upper-case?"},
  {scm_char_lower_case_q, "char-lower-case?"},
  {scm_digit_value, "digit-value"},
  {scm_char_a_integer, "char->integer"},
  {scm_integer_a_char, "integer->char"},
  {scm_string_q, "string?"},
  {scm_make_string, "make-string"},
  {scm_string_length, "string-length"},
  {scm_string_ref, "string-ref"},
  {scm_string_set_b, "string-set!"},
  {scm_string_eq_q, "string=?"},
  {scm_string_ci_eq_q, "string-ci=?"},
  {scm_string_lt_q, "string<?"},
  {scm_string_ci_lt_q, "string-ci<?"},
  {scm_string_gt_q, "string>?"},
  {scm_string_ci_gt_q, "string-ci>?"},
  {scm_string_lte_q, "string<=?"},
  {scm_string_ci_lte_q, "string-ci<=?"},
  {scm_string_gte_q, "string>=?"},
  {scm_string_ci_gte_q, "string-ci>=?"},
  {scm_string_upcase, "string-upcase"},
  {scm_string_downcase, "string-downcase"},
  {scm_substring, "substring"},
  {scm_string_copy, "string-copy"},
  {scm_vector_q, "vector?"},
  {scm_make_vector, "make-vector"},
  {scm_vector_length, "vector-length"},
  {scm_vector_ref, "vector-ref"},
  {scm_vector_set_b, "vector-set!"},
  {scm_vector_a_list, "vector->list"},
  {scm_list_a_vector, "list->vector"},
  {scm_vector_a_string, "vector->string"},
  {scm_string_a_vector, "string->vector"},
  {scm_vector_copy, "vector-copy"},
  {scm_bytevector_q, "bytevector?"},
  {scm_make_bytevector, "make-bytevector"},
  {scm_bytevector_length, "bytevector-length"},
  {scm_bytevector_u8_ref, "bytevector-u8-ref"},
  {scm_bytevector_u8_set_b, "bytevector-u8-set!"},
  {scm_procedure_q, "procedure?"},
  {scm_with_exception_handler, "with-exception-handler"},
  {scm_raise, "raise"},
  {scm_write, "write"},
  {scm_file_exists_q, "file-exists?"},
  {scm_delete_file, "delete-file"},
  {scm_command_line, "command-line"},
  {scm_exit, "exit"},
  {scm_get_environment_variable, "get-environment-variable"},
  {scm_get_environment_variables, "get-environment-variables"},
  {scm_current_second, "current-second"},
  {NULL, NULL}
};
