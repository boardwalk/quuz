#include "quuz.h"
#include <assert.h>

#define ALIGNED __attribute__ ((aligned (8)))
#define QZ_DEF_CFUN(n) static ALIGNED qz_obj_t n(qz_state_t* st, qz_obj_t args)

static int qz_compare(qz_obj_t a, qz_obj_t b)
{
  return a.value - b.value;
}

static void set_var(qz_state_t* st, qz_obj_t name, qz_obj_t value)
{
  qz_obj_t outer_env = qz_to_cell(st->env)->value.pair.first;
  qz_obj_t* inner_env = &qz_to_cell(outer_env)->value.pair.first;
  qz_hash_set(st, inner_env, name, value);
}

typedef int (*pred_fun)(qz_obj_t);

qz_obj_t inner_predicate(qz_state_t* st, qz_obj_t args, pred_fun pf)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));
  int b = pf(value);
  qz_unref(st, value);
  return b ? QZ_TRUE : QZ_FALSE;
}

static void eval2(qz_state_t* st, qz_obj_t *args, qz_obj_t* value1, qz_obj_t* value2)
{
  qz_obj_t expr1 = qz_required_arg(st, args);
  qz_obj_t expr2 = qz_required_arg(st, args);

  *value1 = qz_eval(st, expr1);

  qz_push_safety(st, *value1);
  *value2 = qz_eval(st, expr2);
  qz_pop_safety(st, 1);
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

typedef int (*cmp_fun)(qz_obj_t, qz_obj_t);

static qz_obj_t inner_compare(qz_state_t* st, qz_obj_t args, cmp_fun cf)
{
  qz_obj_t result1, result2;
  eval2(st, &args, &result1, &result2);

  int eq = cf(result1,result2);

  qz_unref(st, result1);
  qz_unref(st, result2);

  return eq ? QZ_TRUE : QZ_FALSE;
}

QZ_DEF_CFUN(scm_eq_q)
{
  return inner_compare(st, args, qz_eq);
}

QZ_DEF_CFUN(scm_eqv_q)
{
  return inner_compare(st, args, qz_eqv);
}

QZ_DEF_CFUN(scm_equal_q)
{
  return inner_compare(st, args, qz_equal);
}

/******************************************************************************
 * 6.2. Numbers
 ******************************************************************************/

/* 6.2.6. Numerical operations */
#define LESS 1
#define EQUAL 2
#define GREATER 4

static int sign_of(int i)
{
  return 2 + 2*(i > 0) - (i < 0);
}

static qz_obj_t inner_num_compare(qz_state_t* st, qz_obj_t args, int flags)
{
  qz_obj_t prev = qz_eval(st, qz_required_arg(st, &args));

  /* compare each pair of numbers */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);
    if(qz_is_none(expr))
      return QZ_TRUE; /* ran out of expressions */

    qz_obj_t curr = qz_eval(st, expr);

    if(!(sign_of(qz_compare(prev, curr)) & flags)) {
      qz_unref(st, prev);
      qz_unref(st, curr);
      return QZ_FALSE; /* comparison didn't match */
    }

    qz_unref(st, prev);
    prev = curr;
  }
}

QZ_DEF_CFUN(scm_num_eq)
{
  return inner_num_compare(st, args, EQUAL);
}

QZ_DEF_CFUN(scm_num_lt)
{
  return inner_num_compare(st, args, LESS);
}

QZ_DEF_CFUN(scm_num_gt)
{
  return inner_num_compare(st, args, GREATER);
}

QZ_DEF_CFUN(scm_num_lte)
{
  return inner_num_compare(st, args, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_num_gte)
{
  return inner_num_compare(st, args, GREATER|EQUAL);
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
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));

  if(qz_eq(value, QZ_FALSE))
    return QZ_TRUE;

  qz_unref(st, value);
  return QZ_FALSE;
}

QZ_DEF_CFUN(scm_boolean_q)
{
  return inner_predicate(st, args, qz_is_bool);
}

/******************************************************************************
 * 6.4. Pairs and lists
 ******************************************************************************/

QZ_DEF_CFUN(scm_pair_q)
{
  return inner_predicate(st, args, qz_is_pair);
}

QZ_DEF_CFUN(scm_cons)
{
  qz_obj_t result1, result2;
  eval2(st, &args, &result1, &result2);

  return qz_make_pair(result1, result2);
}

QZ_DEF_CFUN(scm_car)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));

  if(!qz_is_pair(value)) {
    qz_unref(st, value);
    return qz_error(st, "expected pair");
  }

  qz_obj_t first = qz_ref(st, qz_first(value));
  qz_unref(st, value);
  return first;
}

QZ_DEF_CFUN(scm_cdr)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));

  if(!qz_is_pair(value)) {
    qz_unref(st, value);
    return qz_error(st, "expected pair");
  }

  qz_obj_t rest = qz_ref(st, qz_rest(value));
  qz_unref(st, value);
  return rest;
}

QZ_DEF_CFUN(scm_set_car_b)
{
  qz_obj_t pair, first;
  eval2(st, &args, &pair, &first);

  if(!qz_is_pair(pair)) {
    qz_unref(st, pair);
    qz_unref(st, first);
    return qz_error(st, "expected pair");
  }

  qz_pair_t* pair_raw = qz_to_pair(pair);
  qz_unref(st, pair_raw->first);
  pair_raw->first = first;

  qz_unref(st, pair);
  return QZ_NONE;
}

QZ_DEF_CFUN(scm_set_cdr_b)
{
  qz_obj_t pair, rest;
  eval2(st, &args, &pair, &rest);

  if(!qz_is_pair(pair)) {
    qz_unref(st, pair);
    qz_unref(st, rest);
    return qz_error(st, "expected pair");
  }

  qz_pair_t* pair_raw = qz_to_pair(pair);
  qz_unref(st, pair_raw->rest);
  pair_raw->rest = rest;

  qz_unref(st, pair);
  return QZ_NONE;
}

QZ_DEF_CFUN(scm_null_q)
{
  return inner_predicate(st, args, qz_is_null);
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
  return inner_predicate(st, args, is_list);
}

QZ_DEF_CFUN(scm_make_list)
{
  qz_obj_t k = qz_eval(st, qz_required_arg(st, &args));
  qz_obj_t fill = qz_eval(st, qz_optional_arg(st, &args)); /* none evals to none */

  if(!qz_is_fixnum(k)) {
    qz_unref(st, k);
    qz_unref(st, fill);
    return qz_error(st, "expected fixnum");
  }

  qz_obj_t result = QZ_NULL;

  for(intptr_t i = qz_to_fixnum(k); i > 0; i--)
    result = qz_make_pair(qz_ref(st, fill), result);

  qz_unref(st, fill);

  return result;
}

QZ_DEF_CFUN(scm_list)
{
  qz_obj_t expr = qz_optional_arg(st, &args);
  if(qz_is_none(expr))
    return QZ_NULL;

  qz_obj_t value = qz_eval(st, expr);
  qz_obj_t result = qz_make_pair(value, QZ_NULL);

  for(;;) {
    expr = qz_optional_arg(st, &args);
    if(qz_is_none(expr))
      return result;

    qz_push_safety(st, result);
    value = qz_eval(st, expr);
    qz_pop_safety(st, 1);

    qz_obj_t inner_result = qz_make_pair(value, QZ_NULL);
    qz_to_pair(result)->rest = inner_result;
    result = inner_result;
  }
}

QZ_DEF_CFUN(scm_length)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));

  intptr_t len = 0;
  for(;;) {
    if(qz_is_null(value))
      return qz_from_fixnum(len);

    if(!qz_is_pair(value))
      return qz_error(st, "expected list");

    len++;
    value = qz_rest(value);
  }
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
  eval2(st, &args, &list, &k);

  if(!qz_is_fixnum(k)) {
    qz_unref(st, list);
    qz_unref(st, k);
    return qz_error(st, "expected fixnum");
  }

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
  eval2(st, &args, &obj, &list);

  qz_push_safety(st, obj);
  qz_push_safety(st, list);

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
  return inner_predicate(st, args, qz_is_sym);
}

QZ_DEF_CFUN(scm_symbol_a_string)
{
  qz_obj_t sym = qz_eval(st, qz_required_arg(st, &args));

  if(!qz_is_sym(sym)) {
    qz_unref(st, sym);
    return qz_error(st, "expected symbol");
  }

  qz_obj_t* str = qz_hash_get(st, st->sym_name, sym);
  assert(str && qz_is_string(*str));

  return qz_ref(st, *str);
}

QZ_DEF_CFUN(scm_string_a_symbol)
{
  qz_obj_t str = qz_eval(st, qz_required_arg(st, &args));

  if(!qz_is_string(str)) {
    qz_unref(st, str);
    return qz_error(st, "expected string");
  }

  return qz_make_sym(st, str);
}

/******************************************************************************
 * 6.13. Input and output
 ******************************************************************************/

QZ_DEF_CFUN(scm_write)
{
  qz_obj_t value = qz_eval(st, qz_required_arg(st, &args));
  qz_write(st, value, -1, stdout);
  qz_unref(st, value);
  return QZ_NULL;
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
  {scm_write, "write"},
  {NULL, NULL}
};

