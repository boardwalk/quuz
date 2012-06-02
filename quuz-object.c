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
int qz_is_vector(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_VECTOR;
}
int qz_is_bytevector(qz_obj_t o) {
  return (o.value & 7) == QZ_PT_BYTEVECTOR;
}
int qz_is_bool(qz_obj_t o) {
  return (o.value & 31) == QZ_PT_BOOL;
}
int qz_is_char(qz_obj_t o) {
  return (o.value & 31) == QZ_PT_CHAR;
}
int qz_is_identifier(qz_obj_t o) {
  return (o.value & 31) == QZ_PT_IDENTIFIER;
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
int qz_is_cfn(qz_obj_t o) {
  if(qz_is_cell(o)) {
    qz_cell_t* c = qz_to_cell(o);
    if(c) return c->type == QZ_CT_CFN;
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
  return o.value >> 5;
}
char qz_to_char(qz_obj_t o) {
  assert(qz_is_char(o));
  return o.value >> 5;
}
qz_pair_t* qz_to_pair(qz_obj_t o) {
  assert(qz_is_pair(o));
  return &qz_to_cell(o)->value.pair;
}
qz_hash_t* qz_to_hash(qz_obj_t o) {
  assert(qz_is_hash(o));
  return &qz_to_cell(o)->value.hash;
}
qz_cfn_t qz_to_cfn(qz_obj_t o) {
  assert(qz_is_cfn(o));
  return qz_to_cell(o)->value.cfn;
}
double qz_to_real(qz_obj_t o) {
  assert(qz_is_real(o));
  return qz_to_cell(o)->value.real;
}

/* qz_from_<type> */
static int pointer_aligned(void* p) {
  return ((size_t)p & 7) == 0;
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
qz_obj_t qz_from_vector(qz_array_t* v) {
  assert(pointer_aligned(v));
  return (qz_obj_t) { (size_t)v | QZ_PT_VECTOR };
}
qz_obj_t qz_from_bytevector(qz_array_t* bv) {
  assert(pointer_aligned(bv));
  return (qz_obj_t) { (size_t)bv | QZ_PT_BYTEVECTOR };
}
qz_obj_t qz_from_bool(int b) {
  return (qz_obj_t) { (b << 5) | QZ_PT_BOOL };
}
qz_obj_t qz_from_char(char c) {
  return (qz_obj_t) { (c << 5) | QZ_PT_CHAR };
}

qz_obj_t qz_make_string(const char* str)
{
  size_t str_size = strlen(str);

  qz_array_t* arr = (qz_array_t*)malloc(sizeof(qz_array_t) + sizeof(char)*str_size);

  arr->size = str_size;
  arr->refcount = 1;
  memcpy(QZ_ARRAY_DATA(arr, char), str, str_size);

  return qz_from_string(arr);
}

qz_obj_t qz_make_pair(qz_obj_t first, qz_obj_t rest)
{
  qz_cell_t* cell = (qz_cell_t*)malloc(sizeof(qz_cell_t));

  cell->type = QZ_CT_PAIR;
  cell->refcount = 1;
  cell->value.pair.first = first;
  cell->value.pair.rest = rest;

  return qz_from_cell(cell);
}

/* convert a name into an identifier
 * name is unrefed */
qz_obj_t qz_make_iden(qz_state_t* st, qz_obj_t str)
{
  qz_obj_t iden = qz_get_hash(st->str_iden, str);

  if(qz_is_nil(iden)) {
    iden = (qz_obj_t) { (st->next_iden++ << 5) | QZ_PT_IDENTIFIER };
    qz_ref(str); /* (1) +1 -2 */
    qz_set_hash(&st->str_iden, str, iden);
    qz_set_hash(&st->iden_str, iden, str);
  }
  else {
    qz_unref(str); /* (1) -1 */
  }

  return iden;
}

qz_obj_t qz_make_cfn(qz_cfn_t cfn)
{
  qz_cell_t* cell = (qz_cell_t*)malloc(sizeof(qz_cell_t));

  cell->type = QZ_CT_CFN;
  cell->refcount = 1;
  cell->value.cfn = cfn;

  return qz_from_cell(cell);
}

qz_obj_t* qz_list_tail(qz_obj_t obj)
{
  qz_cell_t* cell = qz_to_cell(obj);

  while(!qz_is_nil(cell->value.pair.rest))
    cell = qz_to_cell(cell->value.pair.rest);

  return &cell->value.pair.first;
}

static void inner_write(qz_state_t* st, qz_obj_t o, int depth, FILE* fp, int* need_space)
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

            inner_write(st, pair->first, depth, fp, need_space);

            if(qz_is_nil(pair->rest))
              break;

            if(!qz_is_pair(pair->rest))
            {
              if(*need_space) fputc(' ', fp);

              fputc('.', fp);
              *need_space = 1;

              inner_write(st, pair->rest, depth, fp, need_space);
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

    // translate identifier to string
    o = qz_get_hash(st->iden_str, o);

    // TODO make this readable by qz_read()
    qz_array_t* s = qz_to_string(o);
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
        inner_write(st, QZ_ARRAY_DATA(v, qz_obj_t)[i], depth, fp, need_space);
      depth++;
    }
    else {
      fputs("...", fp);
    }

    fputc(')', fp);
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

    fputc(')', fp);
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
void qz_write(qz_state_t* st, qz_obj_t o, int depth, FILE* fp)
{
  int need_space = 0;
  inner_write(st, o, depth, fp, &need_space);
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
  /* covers fixnum, identifier, bool, char, and equivalent pointers to other types */
  if(a.value == b.value)
    return 1;

  int a_tag = a.value & 7;
  int b_tag = b.value & 7;

  /* different pointer tags? not equal */
  if(a_tag != b_tag)
    return 0;

  if(a_tag == QZ_PT_EVEN_FIXNUM
    || a_tag == QZ_PT_ODD_FIXNUM
    || a_tag == QZ_PT_SHORT_IMM)
  {
    /* straight compare failed, not equal */
    return 0;
  }
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
    else if(a_cell->type == QZ_CT_CFN)
    {
      /* straight compare cfns */
      return a_cell->value.cfn == b_cell->value.cfn;
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

