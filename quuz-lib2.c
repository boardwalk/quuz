#include "quuz.h"

#define ALIGNED __attribute__ ((aligned (8)))
#define QZ_DEF_CFUN(n) static ALIGNED qz_obj_t n(qz_state_t* st, qz_obj_t args)

static qz_obj_t qz_required_arg(qz_state_t* st, qz_obj_t* obj)
{
  if(!qz_is_pair(*obj))
    qz_error(st, "expected list", *obj);
  qz_pair_t* pair = qz_to_pair(*obj);
  *obj = pair->rest;
  return pair->first;
}

static qz_obj_t qz_optional_arg(qz_state_t* st, qz_obj_t* obj)
{
  if(!qz_is_pair(*obj))
    return QZ_NIL;
  qz_pair_t* pair = qz_to_pair(*obj);
  *obj = pair->rest;
  return pair->first;
}

static qz_obj_t* qz_lookup(qz_state_t* st, qz_obj_t var)
{
  /* TODO */
  return NULL;
}

static int qz_eqv(qz_obj_t a, qz_obj_t b)
{
  /* TODO */
  return a.value = b.value;
}

/* r7rs 4.1. Primitive expression types */

QZ_DEF_CFUN(qz_scm_quote) /* 4.1.2. Literal expressions */
{
  return qz_ref(st, qz_first(args));
}

QZ_DEF_CFUN(qz_scm_lambda) /* 4.1.4. Procedures */
{
  qz_obj_t formals = qz_required_arg(st, &args);
  qz_obj_t body = args;

  qz_cell_t* cell = qz_make_cell(QZ_CT_FUN, 0);

  cell->value.pair.first = qz_ref(st, formals);
  cell->value.pair.rest = qz_ref(st, body);

  return qz_from_cell(cell);
}

QZ_DEF_CFUN(qz_scm_if) /* 4.1.5. Conditionals */
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t consequent = qz_required_arg(st, &args);
  qz_obj_t alternate = qz_optional_arg(st, &args);

  qz_obj_t test_result = qz_eval(st, test);

  if(qz_eqv(test_result, QZ_FALSE))
  {
    if(qz_eqv(alternate, QZ_NIL))
      return QZ_NIL;

    return qz_eval(st, alternate);
  }

  qz_unref(st, test_result);

  return qz_eval(st, consequent);
}

QZ_DEF_CFUN(qz_scm_set_b) /* 4.1.6. Assignments */
{
  qz_obj_t var = qz_required_arg(st, &args);
  qz_obj_t expr = qz_required_arg(st, &args);

  qz_obj_t* slot = qz_lookup(st, var);
  qz_obj_t value = qz_eval(st, expr);

  qz_unref(st, *slot);
  *slot = value;

  return QZ_NIL;
}

/* r7rs 4.2. Derived expression types */

QZ_DEF_CFUN(qz_scm_cond) /* 4.2.1. Conditions */
{
  qz_obj_t clause;
  qz_obj_t result = QZ_NIL;

  /* find matching clause */
  for(;;) {
    clause = qz_optional_arg(st, &args);

    if(qz_eqv(clause, QZ_NIL))
      return QZ_NIL; /* ran out of clauses */

    qz_obj_t test = qz_required_arg(st, &clause);

    if(qz_eqv(test, st->else_sym))
      break; /* hit else clause */

    result = qz_eval(st, qz_required_arg(st, &clause));

    if(!qz_eqv(result, QZ_FALSE))
      break; /* hit true clause */
  }

  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_eqv(expr, QZ_NIL))
    return result; /* clause with only test */

  if(qz_eqv(expr, st->arrow_sym)) {
    expr = qz_required_arg(st, &clause); /* skip arrow */
    return qz_eval(st, expr); /* only one expression allowed */
  }

  /* eval expressions in clause */
  for(;;) {
    qz_unref(st, result); /* from test or previous expression */
    result = qz_eval(st, expr);

    expr = qz_optional_arg(st, &clause);
    if(qz_eqv(expr, QZ_NIL))
      return result; /* ran out of expressions in clause */
  }
}

QZ_DEF_CFUN(qz_scm_case)
{
  qz_obj_t key = qz_eval(st, qz_required_arg(st, &args));

  /* find matching clause */
  qz_obj_t clause;

  for(;;) {
    clause = qz_optional_arg(st, &args);

    if(qz_eqv(clause, QZ_NIL))
      break; /* ran out of clauses */

    if(qz_eqv(clause, st->else_sym))
      break; /* hit else clause */

    qz_obj_t datum_list = qz_required_arg(st, &clause);

    /* match against each datum in list */
    for(;;) {
      qz_obj_t datum = qz_optional_arg(st, &datum_list);

      if(qz_eqv(datum, QZ_NIL)) {
        clause = QZ_NIL;
        break; /* ran out of datum */
      }

      if(qz_eqv(datum, key))
        break; /* hit matching clause */
    }
    if(!qz_eqv(clause, QZ_NIL))
      break; /* hit matching clause */
  }

  qz_unref(st, key);

  if(qz_eqv(clause, QZ_NIL))
    return QZ_NIL;

  /* eval expressions in clause */
  qz_obj_t expr = qz_optional_arg(st, &clause);

  if(qz_eqv(expr, st->arrow_sym)) {
    expr = qz_required_arg(st, &clause); /* skip array */
    return qz_eval(st, expr); /* only one expression allowed */
  }

  qz_obj_t result = QZ_NIL;

  for(;;) {
    if(qz_eqv(expr, QZ_NIL))
      return result; /* ran out of expressions in clause */

    qz_unref(st, result); /* from previous expression */
    result = qz_eval(st, expr);

    expr = qz_optional_arg(st, &clause);
  }
}

QZ_DEF_CFUN(qz_scm_and)
{
  qz_obj_t result = QZ_TRUE;

  /* eval tests */
  for(;;) {
    qz_obj_t test = qz_optional_arg(st, &args);

    if(qz_eqv(test, QZ_NIL))
      return result; /* ran out of tests */

    qz_unref(st, result); /* from previous test */
    result = qz_eval(st, test);

    if(qz_eqv(result, QZ_FALSE))
      return QZ_FALSE; /* not all expressions true */
  }
}

QZ_DEF_CFUN(qz_scm_or)
{
  /* eval tests */
  for(;;) {
    qz_obj_t test = qz_optional_arg(st, &args);

    if(qz_eqv(test, QZ_NIL))
      return QZ_FALSE; /* ran out of tests */

    qz_obj_t result = qz_eval(st, test);

    if(!qz_eqv(result, QZ_FALSE))
      return result; /* not all expressions false */
  }
}

QZ_DEF_CFUN(qz_scm_when)
{
  qz_obj_t test = qz_required_arg(st, &args);
  qz_obj_t result = qz_eval(st, test);

  if(qz_eqv(result, QZ_FALSE))
    return QZ_NIL; /* test was false */

  qz_unref(st, result);

  /* eval expressions */
  for(;;) {
    qz_obj_t expr = qz_optional_arg(st, &args);

    if(qz_eqv(expr, QZ_NIL))
      return QZ_NIL; /* ran out of expressions */

    qz_unref(st, qz_eval(st, expr));
  }
}

QZ_DEF_CFUN(qz_scm_unless)
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

    if(qz_eqv(expr, QZ_NIL))
      return QZ_NIL; /* ran out of expressions */

    qz_unref(st, qz_eval(st, expr));
  }
}

