#include "quuz.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

qz_obj_t const QZ_NIL = { (size_t)NULL | QZ_PT_CELL };
qz_obj_t const QZ_TRUE = { (1 << 4) | QZ_PT_BOOL };
qz_obj_t const QZ_FALSE = { (0 << 4) | QZ_PT_BOOL };

/* qz_is_<type> */
int qz_is_nil(qz_obj_t o) {
  return o.value == QZ_NIL.value;
}
int qz_is_fixnum(qz_obj_t o) {
  return (o.value & 3) == 0;
}
int qz_is_cell(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_CELL;
}
int qz_is_string(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_STRING;
}
int qz_is_identifier(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_IDENTIFIER;
}
int qz_is_vector(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_VECTOR;
}
int qz_is_bytevector(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_BYTEVECTOR;
}
int qz_is_bool(qz_obj_t o) {
  return (o.value & 15) == QZ_PT_BOOL;
}
int qz_is_char(qz_obj_t o) {
  return (o.value & 15) == QZ_PT_CHAR;
}
int qz_is_pair(qz_obj_t o) {
  if(qz_is_cell(o)) {
    qz_cell_t* c = qz_to_cell(o);
    if(c) return c->type == QZ_CT_PAIR;
  }
  return 0;
}
int qz_is_real(qz_obj_t o) {
  if(qz_is_cell(o)) {
    qz_cell_t* c = qz_to_cell(o);
    if(c) return c->type == QZ_CT_REAL;
  }
  return 0;
}

/* qz_to_<type> */
size_t qz_to_fixnum(qz_obj_t o) {
  assert(qz_is_fixnum(o));
  return o.value >> 2;
}
qz_cell_t* qz_to_cell(qz_obj_t o) {
  assert(qz_is_cell(o));
  return (qz_cell_t*)(o.value & ~7);
}
qz_string_t* qz_to_string(qz_obj_t o) {
  assert(qz_is_string(o));
  return (qz_string_t*)(o.value & ~7);
}
qz_string_t* qz_to_identifier(qz_obj_t o) {
  assert(qz_is_identifier(o));
  return (qz_string_t*)(o.value & ~7);
}
qz_vector_t* qz_to_vector(qz_obj_t o) {
  assert(qz_is_vector(o));
  return (qz_vector_t*)(o.value & ~7);
}
qz_bytevector_t* qz_to_bytevector(qz_obj_t o) {
  assert(qz_is_bytevector(o));
  return (qz_bytevector_t*)(o.value & ~7);
}
int qz_to_bool(qz_obj_t o) {
  assert(qz_is_bool(o));
  return o.value >> 4;
}
char qz_to_char(qz_obj_t o) {
  assert(qz_is_char(o));
  return o.value >> 4;
}
qz_pair_t* qz_to_pair(qz_obj_t o) {
  assert(qz_is_pair(o));
  return &qz_to_cell(o)->pair;
}
double qz_to_real(qz_obj_t o) {
  assert(qz_is_real(o));
  return qz_to_cell(o)->real;
}

/* qz_from_<type> */
static int pointer_aligned(void* v) {
  return ((size_t)v & 7) == 0;
}
qz_obj_t qz_from_fixnum(size_t i) {
  return (qz_obj_t) { i << 2 };
}
qz_obj_t qz_from_cell(qz_cell_t* c) {
  assert(pointer_aligned(c));
  return (qz_obj_t) { (size_t)c | QZ_PT_CELL };
}
qz_obj_t qz_from_string(qz_string_t* s) {
  assert(pointer_aligned(s));
  return (qz_obj_t) { (size_t)s | QZ_PT_STRING };
}
qz_obj_t qz_from_identifier(qz_string_t* s) {
  assert(pointer_aligned(s));
  return (qz_obj_t) { (size_t)s | QZ_PT_IDENTIFIER };
}
qz_obj_t qz_from_vector(qz_vector_t* v) {
  assert(pointer_aligned(v));
  return (qz_obj_t) { (size_t)v | QZ_PT_VECTOR };
}
qz_obj_t qz_from_bytevector(qz_bytevector_t* bv) {
  assert(pointer_aligned(bv));
  return (qz_obj_t) { (size_t)bv | QZ_PT_BYTEVECTOR };
}
qz_obj_t qz_from_bool(int b) {
  return (qz_obj_t) { (b << 4) | QZ_PT_BOOL };
}
qz_obj_t qz_from_char(char c) {
  return (qz_obj_t) { (c << 4) | QZ_PT_CHAR };
}

/* writes a qz_obj_t to a file stream in Scheme form
 * when depth < 0, the entire tree will be printed and readable by qz_read
 * when depth >= 0, the tree will only be printed to that depth */
void qz_write(qz_obj_t o, int depth, FILE* fp)
{
  if(qz_is_fixnum(o))
  {
    fprintf(fp, " %lu", qz_to_fixnum(o));
  }
  else if(qz_is_cell(o))
  {
    qz_cell_t* c = qz_to_cell(o);

    if(c)
    {
      if(c->type == QZ_CT_PAIR)
      {
        qz_pair_t* pair = &c->pair;

        fputs(" (", fp);
        if(depth) {
          depth--;
          for(;;) {
            if(qz_is_nil(pair->first)) /* this shouldn't happen? */
              break;

            qz_write(pair->first, depth, fp);

            if(qz_is_nil(pair->rest))
              break;

            if(!qz_is_pair(pair->rest)) {
              fputs(" .", fp);
              qz_write(pair->rest, depth, fp);
              break;
            }

            pair = qz_to_pair(pair->rest);
          }
          depth++;
        }
        else {
          fputs(" ...",  fp);
        }
        fputs(" )", fp);
      }
      else if(c->type == QZ_CT_REAL)
      {
        // TODO make this readable by qz_read()
        fprintf(fp, " %f", c->real);
      }
      else
      {
        assert(0); /* unknown cell type */
      }
    }
    else
    {
      /* nothing if null! */
    }
  }
  else if(qz_is_string(o))
  {
    qz_string_t* s = qz_to_string(o);
    fputc(' ', fp); fputc('"', fp);
    for(size_t i = 0; i < s->size; i++) {
      if(s->data[i] == '"')
        fputs("\\\"", fp);
      else if(s->data[i] == '\\')
        fputs("\\\\", fp);
      else if(isprint(s->data[i]))
        fputc(s->data[i], fp);
      else
        fprintf(fp, "\\x%02x;", s->data[i]);
    }
    fputc('"', fp);
  }
  else if(qz_is_identifier(o))
  {
    // TODO make this readable by qz_read()
    qz_string_t* s = qz_to_identifier(o);
    fprintf(fp, " %.*s", s->size, s->data);
  }
  else if(qz_is_vector(o))
  {
    qz_vector_t* v = qz_to_vector(o);
    fputs(" #(", fp);
    if(depth) {
      depth--;
      for(size_t i = 0; i < v->size; i++)
        qz_write(v->data[i], depth, fp);
      depth++;
    }
    else {
      fputs(" ...", fp);
    }
    fputs(" )", fp);
  }
  else if(qz_is_bytevector(o))
  {
    qz_bytevector_t* bv = qz_to_bytevector(o);
    fputs(" #u8(", fp);
    for(size_t i = 0; i < bv->size; i++)
      fprintf(fp, " #x%02x", bv->data[i]);
    fputs(" )", fp);
  }
  else if(qz_is_bool(o))
  {
    int b = qz_to_bool(o);
    if(b)
      fputs(" #t", fp);
    else
      fputs(" #f", fp);
  }
  else if(qz_is_char(o))
  {
    char c = qz_to_char(o);
    if(isgraph(c))
      fprintf(fp, " #\\%c", c);
    else
      fprintf(fp, " #\\x%02x", c);
  }
  else
  {
    assert(0); /* unknown tagged pointer type */
  }
}

void qz_destroy(qz_obj_t o)
{
  if(qz_is_cell(o))
  {
    qz_cell_t* c = qz_to_cell(o);
    if(c) {
      if(c->type == QZ_CT_PAIR) {
        qz_destroy(c->pair.first);
        qz_destroy(c->pair.rest);
      }
      free(c);
    }
  }
  else if(qz_is_string(o))
  {
    qz_string_t* s = qz_to_string(o);
    if(s->size) free(s);
  }
  else if(qz_is_identifier(o))
  {
    qz_string_t* s = qz_to_identifier(o);
    if(s->size) free(s);
  }
  else if(qz_is_vector(o))
  {
    qz_vector_t* v = qz_to_vector(o);
    if(v->size) {
      size_t i = 0;
      do {
        qz_destroy(v->data[i]);
      }
      while(++i < v->size);
      free(v);
    }
  }
  else if(qz_is_bytevector(o))
  {
    qz_bytevector_t* bv = qz_to_bytevector(o);
    if(bv->size) free(bv);
  }
}

