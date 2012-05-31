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
int qz_is_hash(qz_obj_t o) {
  if(qz_is_cell(o)) {
    qz_cell_t* c = qz_to_cell(o);
    if(c) return c->type == QZ_CT_HASH;
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
intptr_t qz_to_fixnum(qz_obj_t o) {
  assert(qz_is_fixnum(o));
  return (intptr_t)o.value >> 2;
}
qz_cell_t* qz_to_cell(qz_obj_t o) {
  assert(qz_is_cell(o));
  return (qz_cell_t*)(o.value & ~7);
}
qz_array_t* qz_to_string(qz_obj_t o) {
  assert(qz_is_string(o));
  return (qz_array_t*)(o.value & ~7);
}
qz_array_t* qz_to_identifier(qz_obj_t o) {
  assert(qz_is_identifier(o));
  return (qz_array_t*)(o.value & ~7);
}
qz_array_t* qz_to_vector(qz_obj_t o) {
  assert(qz_is_vector(o));
  return (qz_array_t*)(o.value & ~7);
}
qz_array_t* qz_to_bytevector(qz_obj_t o) {
  assert(qz_is_bytevector(o));
  return (qz_array_t*)(o.value & ~7);
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
qz_hash_t* qz_to_hash(qz_obj_t o) {
  assert(qz_is_hash(o));
  return &qz_to_cell(o)->hash;
}
double qz_to_real(qz_obj_t o) {
  assert(qz_is_real(o));
  return qz_to_cell(o)->real;
}

/* qz_from_<type> */
static int pointer_aligned(void* v) {
  return ((size_t)v & 7) == 0;
}
qz_obj_t qz_from_fixnum(intptr_t i) {
  return (qz_obj_t) { i << 2 };
}
qz_obj_t qz_from_cell(qz_cell_t* c) {
  assert(pointer_aligned(c));
  return (qz_obj_t) { (size_t)c | QZ_PT_CELL };
}
qz_obj_t qz_from_string(qz_array_t* s) {
  assert(pointer_aligned(s));
  return (qz_obj_t) { (size_t)s | QZ_PT_STRING };
}
qz_obj_t qz_from_identifier(qz_array_t* s) {
  assert(pointer_aligned(s));
  return (qz_obj_t) { (size_t)s | QZ_PT_IDENTIFIER };
}
qz_obj_t qz_from_vector(qz_array_t* v) {
  assert(pointer_aligned(v));
  return (qz_obj_t) { (size_t)v | QZ_PT_VECTOR };
}
qz_obj_t qz_from_bytevector(qz_array_t* bv) {
  assert(pointer_aligned(bv));
  return (qz_obj_t) { (size_t)bv | QZ_PT_BYTEVECTOR };
}
qz_obj_t qz_from_bool(int b) {
  return (qz_obj_t) { (b << 4) | QZ_PT_BOOL };
}
qz_obj_t qz_from_char(char c) {
  return (qz_obj_t) { (c << 4) | QZ_PT_CHAR };
}

static void inner_write(qz_obj_t o, int depth, FILE* fp, int* need_space)
{
  if(qz_is_fixnum(o))
  {
    if(*need_space) fputc(' ', fp);
    fprintf(fp, "%ld", qz_to_fixnum(o));
    *need_space = 1;
  }
  else if(qz_is_cell(o))
  {
    qz_cell_t* c = qz_to_cell(o);

    if(c)
    {
      if(c->type == QZ_CT_PAIR)
      {
        qz_pair_t* pair = &c->pair;

        if(*need_space) fputc(' ', fp);
        fputc('(', fp);
        *need_space = 0;

        if(depth)
        {
          depth--;
          for(;;)
          {
            if(qz_is_nil(pair->first)) /* this shouldn't happen? */
              break;

            inner_write(pair->first, depth, fp, need_space);

            if(qz_is_nil(pair->rest))
              break;

            if(!qz_is_pair(pair->rest))
            {
              if(*need_space) fputc(' ', fp);

              fputc('.', fp);
              *need_space = 1;

              inner_write(pair->rest, depth, fp, need_space);
              break;
            }

            pair = qz_to_pair(pair->rest);
          }
          depth++;
        }
        else
        {
          fputs("...",  fp);
        }

        fputc(')', fp);
        *need_space = 1;
      }
      else if(c->type == QZ_CT_REAL)
      {
        // TODO make this readable by qz_read()
        if(*need_space) fputc(' ', fp);

        fprintf(fp, "%f", c->real);
        *need_space = 1;
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
    if(*need_space) fputc(' ', fp);

    qz_array_t* s = qz_to_string(o);
    fputc('"', fp);
    for(size_t i = 0; i < s->size; i++) {
      char c = QZ_ARRAY_DATA(s, char)[i];
      if(c == '"')
        fputs("\\\"", fp);
      else if(c == '\\')
        fputs("\\\\", fp);
      else if(isprint(c))
        fputc(c, fp);
      else
        fprintf(fp, "\\x%02x;", c);
    }

    fputc('"', fp);
    *need_space = 1;
  }
  else if(qz_is_identifier(o))
  {
    if(*need_space) fputc(' ', fp);

    // TODO make this readable by qz_read()
    qz_array_t* s = qz_to_identifier(o);
    fprintf(fp, "%.*s", s->size, QZ_ARRAY_DATA(s, char));

    *need_space = 1;
  }
  else if(qz_is_vector(o))
  {
    if(*need_space) fputc(' ', fp);

    fputs("#(", fp);
    *need_space = 0;

    qz_array_t* v = qz_to_vector(o);
    if(depth) {
      depth--;
      for(size_t i = 0; i < v->size; i++)
        inner_write(QZ_ARRAY_DATA(v, qz_obj_t)[i], depth, fp, need_space);
      depth++;
    }
    else {
      fputs("...", fp);
    }

    fputs(")", fp);
    *need_space = 1;
  }
  else if(qz_is_bytevector(o))
  {
    if(*need_space) fputc(' ', fp);

    fputs("#u8(", fp);
    *need_space = 0;

    qz_array_t* bv = qz_to_bytevector(o);
    for(size_t i = 0; i < bv->size; i++)
    {
      if(*need_space) fputc(' ', fp);

      fprintf(fp, "#x%02x", QZ_ARRAY_DATA(bv, uint8_t)[i]);
      *need_space = 1;
    }

    fputs(")", fp);
    *need_space = 1;
  }
  else if(qz_is_bool(o))
  {
    if(*need_space) fputc(' ', fp);

    int b = qz_to_bool(o);
    if(b)
      fputs("#t", fp);
    else
      fputs("#f", fp);

    *need_space = 1;
  }
  else if(qz_is_char(o))
  {
    if(*need_space) fputc(' ', fp);

    char c = qz_to_char(o);
    if(isgraph(c))
      fprintf(fp, "#\\%c", c);
    else
      fprintf(fp, "#\\x%02x", c);

    *need_space = 1;
  }
  else
  {
    assert(0); /* unknown tagged pointer type */
  }
}

/* writes a qz_obj_t to a file stream in Scheme form
 * when depth < 0, the entire tree will be printed and readable by qz_read
 * when depth >= 0, the tree will only be printed to that depth */
void qz_write(qz_obj_t o, int depth, FILE* fp)
{
  int need_space = 0;
  inner_write(o, depth, fp, &need_space);
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
      if(c->type == QZ_CT_HASH) {
        for(size_t i = 0; i < c->hash.capacity; i++)
          qz_destroy(QZ_HASH_DATA(&c->hash)[i]);
      }
      free(c);
    }
  }
  else if(qz_is_string(o))
  {
    qz_array_t* s = qz_to_string(o);
    if(s->size) free(s);
  }
  else if(qz_is_identifier(o))
  {
    qz_array_t* s = qz_to_identifier(o);
    if(s->size) free(s);
  }
  else if(qz_is_vector(o))
  {
    qz_array_t* v = qz_to_vector(o);
    if(v->size) {
      size_t i = 0;
      do {
        qz_destroy(QZ_ARRAY_DATA(v, qz_obj_t)[i]);
      }
      while(++i < v->size);
      free(v);
    }
  }
  else if(qz_is_bytevector(o))
  {
    qz_array_t* bv = qz_to_bytevector(o);
    if(bv->size) free(bv);
  }
}

