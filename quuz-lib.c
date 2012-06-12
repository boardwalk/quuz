#include "quuz.h"

#define ALIGNED __attribute__ ((aligned (8)))
#define QZ_DEF_CFUN(n) static ALIGNED qz_obj_t n(qz_state_t* st, qz_obj_t args)

static qz_obj_t* qz_lookup(qz_state_t* st, qz_obj_t var)
{
  /* TODO */
  return NULL;
}

static int qz_compare(qz_obj_t a, qz_obj_t b)
{
  return a.value - b.value;
}

static void set_var(qz_state_t* st, qz_obj_t name, qz_obj_t value)
{
  qz_obj_t outer_env = qz_to_cell(st->env)->value.pair.first;
  qz_obj_t* inner_env = &qz_to_cell(outer_env)->value.pair.first;
  qz_set_hash(st, inner_env, name, value);
}

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

  qz_obj_t f = qz_from_cell(cell);

  return f;
}

/* 4.1.5. Conditionals */
QZ_DEF_CFUN(scm_if)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t consequent = qz_required_arg(st, &args);
  qz_obj_t alternate = qz_optional_arg(st, &args);

  qz_obj_t test_result = qz_eval(st, test);

  if(qz_eqv(test_result, QZ_FALSE))
  {
    if(qz_is_nil(alternate))
      return QZ_NIL;

    return qz_eval(st, alternate);
  }

  qz_unref(st, test_result);

  return qz_eval(st, consequent);
}

/* 4.1.6. Assignments */
QZ_DEF_CFUN(scm_set_b)
{
  qz_obj_t var = qz_required_arg(st, &args);
  qz_obj_t expr = qz_required_arg(st, &args);

  qz_obj_t* slot = qz_lookup(st, var);
  qz_obj_t value = qz_eval(st, expr);

  qz_unref(st, *slot);
  *slot = value;

  return QZ_NIL;
}

/******************************************************************************
 * 4.2. Derived expression types
 ******************************************************************************/

/* 4.2.1. Conditions */
QZ_DEF_CFUN(scm_cond)
{
  qz_obj_t clause;
  qz_obj_t result = QZ_NIL;

  /* find matching clause */
  for(;;) {
    clause = qz_optional_arg(st, &args);

    if(qz_is_nil(clause))
      return QZ_NIL; /* ran out of clauses */

    qz_obj_t test = qz_required_arg(st, &clause);

    if(qz_eqv(test, st->else_sym))
      break; /* hit else clause */

    /* no need to unref previous result, it must be nil or false */
    result = qz_eval(st, test);

    if(!qz_eqv(result, QZ_FALSE))
      break; /* hit true clause */
  }

  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_is_nil(expr))
    return result; /* clause with only test */

  if(qz_eqv(expr, st->arrow_sym)) {
    /* get function */
    //qz_protect(st, result);
    qz_obj_t fun = qz_required_arg(st, &clause);
    //qz_unprotect(st);
    /* call function */
    qz_obj_t fun_call = qz_make_pair(qz_ref(st, fun), qz_make_pair(result, QZ_NIL));
    //qz_protect(st, fun_call);
    result = qz_eval(st, fun_call);
    //qz_unprotect(st);
    qz_unref(st, fun_call);
    return result;
  }

  /* eval expressions in clause */
  for(;;) {
    qz_unref(st, result); /* from test or previous expression */
    result = qz_eval(st, expr);

    expr = qz_optional_arg(st, &clause);
    if(qz_is_nil(expr))
      return result; /* ran out of expressions in clause */
  }
}

QZ_DEF_CFUN(scm_case)
{
  qz_obj_t key = qz_eval(st, qz_required_arg(st, &args));

  /* find matching clause */
  qz_obj_t clause;

  for(;;) {
    clause = qz_optional_arg(st, &args);

    if(qz_is_nil(clause))
      break; /* ran out of clauses */

    if(qz_eqv(clause, st->else_sym))
      break; /* hit else clause */

    qz_obj_t datum_list = qz_required_arg(st, &clause);

    /* match against each datum in list */
    for(;;) {
      qz_obj_t datum = qz_optional_arg(st, &datum_list);

      if(qz_is_nil(datum)) {
        clause = QZ_NIL;
        break; /* ran out of datum */
      }

      if(qz_eqv(datum, key))
        break; /* hit matching clause */
    }
    if(!qz_is_nil(clause))
      break; /* hit matching clause */
  }

  qz_unref(st, key);

  if(qz_is_nil(clause))
    return QZ_NIL;

  /* eval expressions in clause */
  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_eqv(expr, st->arrow_sym)) {
    expr = qz_required_arg(st, &clause); /* skip arrow */
    return qz_eval(st, expr); /* only one expression allowed */
  }

  qz_obj_t result = QZ_NIL;

  for(;;) {
    if(qz_is_nil(expr))
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

    if(qz_is_nil(test))
      return result; /* ran out of tests */

    qz_unref(st, result); /* from previous test */
    result = qz_eval(st, test);

    if(qz_eqv(result, QZ_FALSE))
      return QZ_FALSE; /* not all expressions true */
  }
}

QZ_DEF_CFUN(scm_or)
{
  /* eval tests */
  for(;;) {
    qz_obj_t test = qz_optional_arg(st, &args);

    if(qz_is_nil(test))
      return QZ_FALSE; /* ran out of tests */

    qz_obj_t result = qz_eval(st, test);

    if(!qz_eqv(result, QZ_FALSE))
      return result; /* not all expressions false */
  }
}

QZ_DEF_CFUN(scm_when)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t result = qz_eval(st, test);

  if(qz_eqv(result, QZ_FALSE))
    return QZ_NIL; /* test was false */

  qz_unref(st, result);

  /* eval expressions */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);

    if(qz_is_nil(expr))
      return QZ_NIL; /* ran out of expressions */

    qz_unref(st, qz_eval(st, expr));
  }
}

QZ_DEF_CFUN(scm_unless)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t result = qz_eval(st, test);

  if(!qz_eqv(result, QZ_FALSE)) {
    qz_unref(st, result);
    return QZ_NIL; /* test was true */
  }

  /* eval expressions */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);

    if(qz_is_nil(expr))
      return QZ_NIL; /* ran out of expressions */

    qz_unref(st, qz_eval(st, expr));
  }
}

/* 4.2.3. Sequencing */

QZ_DEF_CFUN(scm_begin)
{
  qz_obj_t result = QZ_NIL;

  /* eval expressions */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);

    if(qz_is_nil(expr))
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
    qz_obj_t var = qz_required_arg(st, &args);

    if(!qz_is_sym(var))
      return qz_error(st, "function variant of define not given symbol");

    qz_cell_t* cell = qz_make_cell(QZ_CT_FUN, 0);

    cell->value.pair.first = qz_ref(st, qz_list_head(st->env));
    cell->value.pair.rest = qz_ref(st, args);

    set_var(st, var, qz_from_cell(cell));
  }
  else
  {
    return qz_error(st, "first argument to define must be a symbol or list");
  }

  return QZ_NIL;

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

static qz_obj_t inner_compare(qz_state_t* st, qz_obj_t args, int flags)
{
  qz_obj_t prev = qz_eval(st, qz_required_arg(st, &args));

  /* compare each pair of numbers */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);
    if(qz_is_nil(expr))
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
  return inner_compare(st, args, EQUAL);
}

QZ_DEF_CFUN(scm_num_lt)
{
  return inner_compare(st, args, LESS);
}

QZ_DEF_CFUN(scm_num_gt)
{
  return inner_compare(st, args, GREATER);
}

QZ_DEF_CFUN(scm_num_lte)
{
  return inner_compare(st, args, LESS|EQUAL);
}

QZ_DEF_CFUN(scm_num_gte)
{
  return inner_compare(st, args, GREATER|EQUAL);
}

QZ_DEF_CFUN(scm_num_add)
{
  qz_obj_t result = qz_from_fixnum(0);

  for(;;)
  {
    qz_obj_t expr = qz_optional_arg(st, &args);
    if(qz_is_nil(expr))
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
    if(qz_is_nil(expr))
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

  if(qz_is_nil(expr))
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
    if(qz_is_nil(expr))
      return result; /* ran out of expressions */
  }
}

/******************************************************************************
 * 6.13. Input and output
 ******************************************************************************/

QZ_DEF_CFUN(scm_write)
{
  qz_obj_t expr = qz_required_arg(st, &args);
  qz_obj_t value = qz_eval(st, expr);
  qz_write(st, value, -1, stdout);
  qz_unref(st, value);
  return QZ_NIL;
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
  {scm_begin, "begin"},
  {scm_define, "define"},
  {scm_num_eq, "="},
  {scm_num_lt, "<"},
  {scm_num_gt, ">"},
  {scm_num_lte, "<="},
  {scm_num_gte, ">="},
  {scm_num_add, "+"},
  {scm_num_mul, "*"},
  {scm_num_sub, "-"},
  {scm_write, "write"},
  {NULL, NULL}
};

