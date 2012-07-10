#include "quuz.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>

static const char* type_name(char t)
{
  switch(t) {
  case 'a': return "any";
  case 'i': return "fixnum";
  case 'f': return "cfun";
  case 'n': return "sym";
  case 'b': return "bool";
  case 'c': return "char";
  case 'p': return "pair";
  case 'g': return "fun";
  case 'q': return "promise";
  case 'e': return "error";
  case 's': return "string";
  case 'v': return "vector";
  case 'w': return "bytevector";
  case 'h': return "hash";
  case 't': return "record";
  case 'd': return "port";
  case 'r': return "real";
  }

  assert(0);
  return "unknown";
}

static int is_type(qz_obj_t obj, char t)
{
  switch(t) {
  case 'a': return !qz_is_none(obj);
  case 'i': return qz_is_fixnum(obj);
  case 'f': return qz_is_cfun(obj);
  case 'n': return qz_is_sym(obj);
  case 'b': return qz_is_bool(obj);
  case 'c': return qz_is_char(obj);
  case 'p': return qz_is_pair(obj);
  case 'g': return qz_is_fun(obj);
  case 'q': return qz_is_promise(obj);
  case 'e': return qz_is_error(obj);
  case 's': return qz_is_string(obj);
  case 'v': return qz_is_vector(obj);
  case 'w': return qz_is_bytevector(obj);
  case 'h': return qz_is_hash(obj);
  case 't': return qz_is_record(obj);
  case 'd': return qz_is_port(obj);
  case 'r': return qz_is_real(obj);
  }

  assert(0);
  return 0;
}

void qz_get_args(qz_state_t* st, qz_obj_t* args, const char* spec, ...)
{
  va_list ap;
  va_start(ap, spec);

  size_t nargs = 0;
  for(const char* s = spec; *s; /**/)
  {
    qz_obj_t* obj = va_arg(ap, qz_obj_t*);

    int type_char = *s++;
    int eval = (*s == '~') ? (s++, 0) : 1;
    int optional = (*s == '?') ? (s++, 1) : 0;

    if(qz_is_pair(*args)) {
      /* pull argument from list and advance */
      *obj = qz_first(*args);
      *args = qz_rest(*args);

      if(eval) {
        /* evaluate argument */
        *obj = qz_eval(st, *obj);
      }
      else {
        /* don't evaluate argument */
        *obj = qz_ref(st, *obj);
      }
      qz_push_safety(st, *obj);
      nargs++;

      /* check argument type */
      if(!is_type(*obj, type_char)) {
        char msg[64];
        sprintf(msg, "expected %s at argument %ld\n", type_name(type_char), nargs);
        qz_error(st, msg, obj, NULL);
      }
    }
    else if(qz_is_null(*args))  {
      /* allow optional arguments */
      if(optional) {
        *obj = QZ_NONE;
        continue; /* missing optional argument */
      }
      char msg[64];
      sprintf(msg, "missing %s at argument %ld\n", type_name(type_char), nargs);
      qz_error(st, msg, NULL);
    }
    else {
      qz_error(st, "improper argument list", args, NULL);
    }
  }
  qz_pop_safety(st, nargs);

  va_end(ap);
}

qz_obj_t qz_eval_list(qz_state_t* st, qz_obj_t list)
{
  qz_obj_t result = QZ_NULL;
  qz_obj_t elem;

  while(qz_is_pair(list)) {
    /* grab argument */
    qz_obj_t inner_elem = qz_first(list);
    list = qz_rest(list);

    /* eval argument */
    qz_push_safety(st, result);
    inner_elem = qz_eval(st, inner_elem);
    qz_pop_safety(st, 1);

    /* append to result */
    inner_elem = qz_make_pair(inner_elem, QZ_NULL);
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

void qz_printf(qz_state_t* st, qz_obj_t port, const char* fmt, ...)
{
  FILE* fp = qz_to_port(port)->fp;

  va_list ap;
  va_start(ap, fmt);

  const char* begin = fmt;
  const char* end = strchr(begin, '%');

  while(end)
  {
    fwrite(begin, sizeof(char), end - begin, fp);
    switch(*(end + 1)) {
    case '%':
      fputc('%', fp);
      begin = end + 2;
      break;
    case 'd':
      qz_display(st, va_arg(ap, qz_obj_t), port);
      begin = end + 2;
      break;
    case 'w':
      qz_write(st, va_arg(ap, qz_obj_t), port);
      begin = end + 2;
      break;
    default:
      begin = end + 1;
      break;
    }
    end = strchr(begin, '%');
  }

  va_end(ap);

  fputs(begin, fp);
}
