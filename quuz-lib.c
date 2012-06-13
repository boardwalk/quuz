#include "quuz.h"

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
    return qz_eval(st, alternate); /* covers no alternate, nil evals to nil */

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

    if(qz_eq(test, st->else_sym))
      break; /* hit else clause */

    /* no need to unref previous result, it must be nil or false */
    result = qz_eval(st, test);

    if(!qz_eq(result, QZ_FALSE))
      break; /* hit true clause */
  }

  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_is_nil(expr))
    return result; /* clause with only test */

  if(qz_eq(expr, st->arrow_sym)) {
    /* get function */
    qz_push_safety(st, result);
    qz_obj_t fun = qz_required_arg(st, &clause);
    qz_pop_safety(st, 1);
    /* call function */
    qz_obj_t fun_call = qz_make_pair(qz_ref(st, fun), qz_make_pair(result, QZ_NIL));
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
    if(qz_is_nil(expr))
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

    if(qz_is_nil(clause))
      break; /* ran out of clauses */

    if(qz_eq(clause, st->else_sym))
      break; /* hit else clause */

    qz_obj_t datum_list = qz_required_arg(st, &clause);

    /* match against each datum in list */
    for(;;) {
      qz_obj_t datum = qz_optional_arg(st, &datum_list);

      if(qz_is_nil(datum)) {
        clause = QZ_NIL;
        break; /* ran out of datum */
      }

      if(qz_eqv(datum, result))
        break; /* hit matching clause */
    }
    if(!qz_is_nil(clause))
      break; /* hit matching clause */
  }

  if(qz_is_nil(clause)) {
    qz_unref(st, result);
    return QZ_NIL; /* no clause found */
  }

  /* eval expressions in clause */
  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_eq(expr, st->arrow_sym)) {
    /* get function */
    qz_push_safety(st, result);
    qz_obj_t fun = qz_required_arg(st, &clause);
    qz_pop_safety(st, 1);
    /* call function */
    qz_obj_t fun_call = qz_make_pair(qz_ref(st, fun), qz_make_pair(result, QZ_NIL));
    qz_push_safety(st, fun_call);
    result = qz_eval(st, fun_call);
    qz_pop_safety(st, 1);
    qz_unref(st, fun_call);
    return result;
  }

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

    if(qz_eq(result, QZ_FALSE))
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

    if(!qz_eq(result, QZ_FALSE))
      return result; /* not all expressions false */
  }
}

QZ_DEF_CFUN(scm_when)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t result = qz_eval(st, test);

  if(qz_eq(result, QZ_FALSE))
    return QZ_NIL; /* test was false */

  qz_unref(st, result);

  /* eval expressions */
  qz_unref(st, scm_begin(st, args));

  return QZ_NIL;
}

QZ_DEF_CFUN(scm_unless)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t result = qz_eval(st, test);

  if(!qz_eq(result, QZ_FALSE)) {
    qz_unref(st, result);
    return QZ_NIL; /* test was true */
  }

  /* eval expressions */
  qz_unref(st, scm_begin(st, args));

  return QZ_NIL;
}

/* 4.2.2. Binding constructs */
QZ_DEF_CFUN(scm_let)
{
  qz_obj_t bindings = qz_required_arg(st, &args);

  /* create frame */
  qz_obj_t frame = qz_make_hash();

  for(;;) {
    qz_obj_t binding = qz_optional_arg(st, &bindings);

    if(qz_is_nil(binding))
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

    if(qz_is_nil(binding))
      break;

    qz_obj_t sym = qz_required_arg(st, &binding);
    qz_obj_t expr = qz_required_arg(st, &binding);

    if(!qz_is_sym(sym))
      qz_error(st, "expected symbol");

    qz_hash_set(st, &qz_to_cell(env)->value.pair.first, sym, qz_eval(st, expr));
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

  return QZ_NIL;

}

/******************************************************************************
 * 6.1. Equivalence predicates
 ******************************************************************************/

typedef int (*cmp_func)(qz_obj_t, qz_obj_t);

static qz_obj_t inner_compare(qz_state_t* st, qz_obj_t args, cmp_func cf)
{
  qz_obj_t expr1 = qz_required_arg(st, &args);
  qz_obj_t expr2 = qz_required_arg(st, &args);

  qz_obj_t result1 = qz_eval(st, expr1);

  qz_push_safety(st, result1);
  qz_obj_t result2 = qz_eval(st, expr2);
  qz_pop_safety(st, 1);

  int eq = cf(expr1, expr2);

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
  {scm_write, "write"},
  {NULL, NULL}
};

