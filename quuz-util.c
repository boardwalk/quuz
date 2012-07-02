#include "quuz.h"
#include <assert.h>
#include <stdarg.h>

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
  for(const char* s = spec; *s; s++)
  {
    qz_obj_t* obj = va_arg(ap, qz_obj_t*);

    if(qz_is_pair(*args)) {
      /* pull argument from list and advance */
      *obj = qz_first(*args);
      *args = qz_rest(*args);

      /* evaluate argument */
      *obj = qz_eval(st, *obj);
      qz_push_safety(st, *obj);
      nargs++;

      /* check argument type */
      if(!is_type(*obj, *s)) {
        char msg[64];
        sprintf(msg, "expected %s at argument %ld\n", type_name(*s), nargs);
        qz_error(st, msg, obj, NULL);
      }

      if(*(s + 1) == '?') {
        s++; /* skip ? */
        /* present optional argument */
      }
    }
    else if(qz_is_null(*args))  {
      if(*(s + 1) == '?') {
        *obj = QZ_NONE;
        s++; /* skip ? */
        continue; /* missing optional argument */
      }
      char msg[64];
      sprintf(msg, "missing %s at argument %ld\n", type_name(*s), nargs);
      qz_error(st, msg, NULL);
    }
    else {
      qz_error(st, "improper argument list", args, NULL);
    }
  }
  qz_pop_safety(st, nargs);

  va_end(ap);
}
