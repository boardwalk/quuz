#include "quuz.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
  return &qz_to_cell(o)->value.pair;
}
qz_hash_t* qz_to_hash(qz_obj_t o) {
  assert(qz_is_hash(o));
  return &qz_to_cell(o)->value.hash;
}
double qz_to_real(qz_obj_t o) {
  assert(qz_is_real(o));
  return qz_to_cell(o)->value.real;
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
        qz_pair_t* pair = &c->value.pair;

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

        fprintf(fp, "%f", c->value.real);
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
    fprintf(fp, "%.*s", (int)s->size, QZ_ARRAY_DATA(s, char));

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

void qz_ref(qz_obj_t obj)
{
  if(qz_is_cell(obj))
  {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell)
      cell->refcount++;
  }
  else if(qz_is_string(obj))
  {
    qz_to_string(obj)->refcount++;
  }
  else if(qz_is_identifier(obj))
  {
    qz_to_identifier(obj)->refcount++;
  }
  else if(qz_is_vector(obj))
  {
    qz_to_vector(obj)->refcount++;
  }
  else if(qz_is_bytevector(obj))
  {
    qz_to_bytevector(obj)->refcount++;
  }
}

void qz_unref(qz_obj_t obj)
{
  if(qz_is_cell(obj))
  {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell && !--cell->refcount) {
      if(cell->type == QZ_CT_PAIR) {
        qz_unref(cell->value.pair.first);
        qz_unref(cell->value.pair.rest);
      }
      else if(cell->type == QZ_CT_HASH) {
        for(size_t i = 0; i < cell->value.hash.capacity; i++) {
          qz_pair_t* pair = QZ_HASH_DATA(&cell->value.hash) + i;
          qz_unref(pair->first);
          qz_unref(pair->rest);
        }
      }
      free(cell); /* I never liked that game */
    }
  }
  else if(qz_is_string(obj))
  {
    qz_array_t* str = qz_to_string(obj);
    if(!--str->refcount)
      free(str);
  }
  else if(qz_is_identifier(obj))
  {
    qz_array_t* iden = qz_to_identifier(obj);
    if(!--iden->refcount)
      free(iden);
  }
  else if(qz_is_vector(obj))
  {
    qz_array_t* vec = qz_to_vector(obj);
    if(!--vec->refcount) {
      for(size_t i = 0; i < vec->size; i++)
        qz_unref(QZ_ARRAY_DATA(vec, qz_obj_t)[i]);
      free(vec);
    }
  }
  else if(qz_is_bytevector(obj))
  {
    qz_array_t* bvec = qz_to_bytevector(obj);
    if(!--bvec->refcount)
      free(bvec);
  }
}

/* deref the object in slot, replacing it with obj
 * obj gains a reference */
void qz_assign(qz_obj_t* slot, qz_obj_t obj)
{
  qz_unref(*slot);
  qz_ref(obj);
  *slot = obj;
}

/* performs a bitwise comparison of two arrays
 * returns nonzero if equal */
static int compare_array(qz_array_t* a, qz_array_t* b, size_t elem_size)
{
  if(a->size != b->size)
    return 0;

  return memcmp(QZ_ARRAY_DATA(a, char), QZ_ARRAY_DATA(b, char), a->size*elem_size) == 0;
}

/* scheme's equal? procedure */
int qz_equal(qz_obj_t a, qz_obj_t b)
{
  /* covers fixnum, bool, char, and equivalent pointers to other types */
  if(a.value == b.value)
    return 1;

  int a_tag = a.value & 7;
  int b_tag = b.value & 7;

  /* different pointer tags? not equal */
  if(a_tag != b_tag)
    return 0;

  if(a_tag == QZ_PT_CELL)
  {
    qz_cell_t* a_cell = qz_to_cell(a);
    qz_cell_t* b_cell = qz_to_cell(b);

    /* different cell types? not equal */
    if(a_cell->type != b_cell->type)
      return 0;

    if(a_cell->type == QZ_CT_PAIR)
    {
      /* recusively compare pairs */
      return qz_equal(a_cell->value.pair.first, b_cell->value.pair.first)
        && qz_equal(b_cell->value.pair.rest, b_cell->value.pair.rest);
    }
    else if(a_cell->type == QZ_CT_REAL)
    {
      /* straight compare reals */
      return a_cell->value.real == b_cell->value.real;
    }
  }
  else if(a_tag == QZ_PT_STRING)
  {
    /* bitwise compare strings */
    return compare_array(qz_to_string(a), qz_to_string(b), sizeof(char));
  }
  else if(a_tag == QZ_PT_IDENTIFIER)
  {
    /* bitwise compare idenifiers */
    return compare_array(qz_to_identifier(a), qz_to_identifier(b), sizeof(char));
  }
  else if(a_tag == QZ_PT_VECTOR)
  {
    /* recursively compare vectors */
    qz_array_t* a_vec = qz_to_vector(a);
    qz_array_t* b_vec = qz_to_vector(b);

    if(a_vec->size != b_vec->size)
      return 0;

    for(size_t i = 0; i < a_vec->size; i++) {
      if(!qz_equal(QZ_ARRAY_DATA(a_vec, qz_obj_t)[i], QZ_ARRAY_DATA(b_vec, qz_obj_t)[i]))
        return 0;
    }

    return 1;
  }
  else if(a_tag == QZ_PT_BYTEVECTOR)
  {
    /* bitwise compare bytevectors */
    return compare_array(qz_to_bytevector(a), qz_to_bytevector(b), sizeof(uint8_t));
  }

  assert(0); /* can't compare this type */
}

