#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define QZ_INFO_BITS (sizeof(size_t)*CHAR_BIT)

qz_obj_t const QZ_NIL = { (size_t)NULL | QZ_PT_CELL };
qz_obj_t const QZ_TRUE = { (1 << 4) | QZ_PT_BOOL };
qz_obj_t const QZ_FALSE = { (0 << 4) | QZ_PT_BOOL };

/* cell->info accessors */
size_t qz_refcount(qz_cell_t* cell) {
  return cell->info & (~(size_t)0 >> 7);
}
qz_cell_type_t qz_type(qz_cell_t* cell) {
  return (cell->info >> (QZ_INFO_BITS - 7)) & 7;
}
qz_cell_color_t qz_color(qz_cell_t* cell) {
  return (cell->info >> (QZ_INFO_BITS - 4)) & 7;
}
size_t qz_buffered(qz_cell_t* cell) {
  return (cell->info >> (QZ_INFO_BITS - 1)) & 1;
}

void qz_set_refcount(qz_cell_t* cell, size_t rc) {
  cell->info = (cell->info & (~(size_t)0 << (QZ_INFO_BITS - 7))) | rc;
}
void qz_set_type(qz_cell_t* cell, qz_cell_type_t ct) {
  cell->info = (cell->info & ~((size_t)7 << (QZ_INFO_BITS - 7))) | ((size_t)ct << (QZ_INFO_BITS - 7));
}
void qz_set_color(qz_cell_t* cell, qz_cell_color_t cc) {
  cell->info = (cell->info & ~((size_t)7 << (QZ_INFO_BITS - 4))) | ((size_t)cc << (QZ_INFO_BITS - 4));
}
void qz_set_buffered(qz_cell_t* cell, size_t bu) {
  cell->info = (cell->info & ~((size_t)1 << (QZ_INFO_BITS - 1))) | (bu << (QZ_INFO_BITS - 1));
}

/* qz_is_<type> */
int qz_is_nil(qz_obj_t obj) {
  return obj.value == QZ_NIL.value;
}
int qz_is_fixnum(qz_obj_t obj) {
  return (obj.value & 3) == 0;
}
int qz_is_cell(qz_obj_t obj) {
  return (obj.value & 7) == QZ_PT_CELL;
}
int qz_is_cfun(qz_obj_t obj) {
  return (obj.value & 7) == QZ_PT_CFUN;
}
int qz_is_sym(qz_obj_t obj) {
  return (obj.value & 31) == QZ_PT_SYM;
}
int qz_is_bool(qz_obj_t obj) {
  return (obj.value & 31) == QZ_PT_BOOL;
}
int qz_is_char(qz_obj_t obj) {
  return (obj.value & 31) == QZ_PT_CHAR;
}
static int cell_of_type(qz_obj_t obj, qz_cell_type_t type) {
  if(qz_is_cell(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell) return qz_type(cell) == type;
  }
  return 0;
}
int qz_is_pair(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_PAIR);
}
int qz_is_fun(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_FUN);
}
int qz_is_string(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_STRING);
}
int qz_is_vector(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_VECTOR);
}
int qz_is_bytevector(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_BYTEVECTOR);
}
int qz_is_hash(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_HASH);
}
int qz_is_real(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_REAL);
}

/* qz_to_<type> */
intptr_t qz_to_fixnum(qz_obj_t obj) {
  assert(qz_is_fixnum(obj));
  return (intptr_t)obj.value >> 2;
}
qz_cell_t* qz_to_cell(qz_obj_t obj) {
  assert(qz_is_cell(obj));
  return (qz_cell_t*)(obj.value & ~(size_t)7);
}
qz_cfun_t qz_to_cfun(qz_obj_t obj) {
  assert(qz_is_cfun(obj));
  return (qz_cfun_t)(obj.value & ~(size_t)7);
}
size_t qz_to_sym(qz_obj_t obj) {
  assert(qz_is_sym(obj));
  return obj.value >> 5;
}
int qz_to_bool(qz_obj_t obj) {
  assert(qz_is_bool(obj));
  return obj.value >> 5;
}
char qz_to_char(qz_obj_t obj) {
  assert(qz_is_char(obj));
  return obj.value >> 5;
}
qz_pair_t* qz_to_pair(qz_obj_t obj) {
  assert(qz_is_pair(obj));
  return &qz_to_cell(obj)->value.pair;
}
qz_array_t* qz_to_string(qz_obj_t obj) {
  assert(qz_is_string(obj));
  return &qz_to_cell(obj)->value.array;
}
qz_array_t* qz_to_vector(qz_obj_t obj) {
  assert(qz_is_vector(obj));
  return &qz_to_cell(obj)->value.array;
}
qz_array_t* qz_to_bytevector(qz_obj_t obj) {
  assert(qz_is_bytevector(obj));
  return &qz_to_cell(obj)->value.array;
}
qz_array_t* qz_to_hash(qz_obj_t obj) {
  assert(qz_is_hash(obj));
  return &qz_to_cell(obj)->value.array;
}
double qz_to_real(qz_obj_t obj) {
  assert(qz_is_real(obj));
  return qz_to_cell(obj)->value.real;
}

/* qz_from_<type> */
static int pointer_aligned(void* ptr) {
  return ((size_t)ptr & 7) == 0;
}
qz_obj_t qz_from_fixnum(intptr_t i) {
  return (qz_obj_t) { i << 2 };
}
qz_obj_t qz_from_cell(qz_cell_t* cell) {
  assert(pointer_aligned(cell));
  return (qz_obj_t) { (size_t)cell | QZ_PT_CELL };
}
qz_obj_t qz_from_cfun(qz_cfun_t cfun) {
  assert(pointer_aligned((void*)(size_t)cfun));
  return (qz_obj_t) { (size_t)cfun | QZ_PT_CFUN };
}
qz_obj_t qz_from_bool(int b) {
  return (qz_obj_t) { (b << 5) | QZ_PT_BOOL };
}
qz_obj_t qz_from_char(char c) {
  return (qz_obj_t) { (c << 5) | QZ_PT_CHAR };
}

qz_cell_t* qz_make_cell(qz_cell_type_t type, size_t extra_size)
{
  qz_cell_t* cell = (qz_cell_t*)malloc(sizeof(qz_cell_t) + extra_size);

  memset(cell, 0, sizeof(qz_cell_t));

  qz_set_refcount(cell, 1);
  qz_set_type(cell, type);
  qz_set_color(cell, QZ_CC_BLACK);
  qz_set_buffered(cell, 0);

  return cell;
}

qz_obj_t qz_make_string(const char* str)
{
  size_t str_size = strlen(str);

  qz_cell_t* cell = qz_make_cell(QZ_CT_STRING, str_size*sizeof(char));

  cell->value.array.size = str_size;
  cell->value.array.capacity = str_size;

  memcpy(QZ_CELL_DATA(cell, char), str, str_size);

  return qz_from_cell(cell);
}

qz_obj_t qz_make_pair(qz_obj_t first, qz_obj_t rest)
{
  qz_cell_t* cell = qz_make_cell(QZ_CT_PAIR, 0);

  cell->value.pair.first = first;
  cell->value.pair.rest = rest;

  return qz_from_cell(cell);
}

/* convert a name into an symbol
 * name is unrefed */
qz_obj_t qz_make_sym(qz_state_t* st, qz_obj_t name)
{
  qz_obj_t sym = qz_get_hash(st->name_sym, name);

  if(qz_is_nil(sym)) {
    sym = (qz_obj_t) { (st->next_sym++ << 5) | QZ_PT_SYM };
    qz_ref(name); /* (1) +1 -2 */
    qz_set_hash(&st->name_sym, name, sym);
    qz_set_hash(&st->sym_name, sym, name);
  }
  else {
    qz_unref(name); /* (1) -1 */
  }

  return sym;
}

qz_obj_t* qz_list_head_ptr(qz_obj_t obj)
{
  assert(qz_is_pair(obj));
  qz_cell_t* cell = qz_to_cell(obj);
  return &cell->value.pair.first;
}

qz_obj_t* qz_list_tail_ptr(qz_obj_t obj)
{
  assert(qz_is_pair(obj));
  qz_cell_t* cell = qz_to_cell(obj);
  while(!qz_is_nil(cell->value.pair.rest))
    cell = qz_to_cell(cell->value.pair.rest);
  return &cell->value.pair.first;
}

qz_obj_t qz_list_head(qz_obj_t obj) { return *qz_list_head_ptr(obj); }
qz_obj_t qz_list_tail(qz_obj_t obj) { return *qz_list_tail_ptr(obj); }

qz_obj_t* qz_vector_head_ptr(qz_obj_t obj)
{
  assert(qz_is_vector(obj));
  qz_cell_t* cell = qz_to_cell(obj);
  assert(cell->value.array.size != 0);
  return QZ_CELL_DATA(cell, qz_obj_t);
}

qz_obj_t* qz_vector_tail_ptr(qz_obj_t obj)
{
  assert(qz_is_vector(obj));
  qz_cell_t* cell = qz_to_cell(obj);
  assert(cell->value.array.size != 0);
  return QZ_CELL_DATA(cell, qz_obj_t) + (cell->value.array.size - 1);
}

qz_obj_t qz_vector_head(qz_obj_t obj) { return *qz_vector_head_ptr(obj); }
qz_obj_t qz_vector_tail(qz_obj_t obj) { return *qz_vector_tail_ptr(obj); }

qz_obj_t qz_ref(qz_obj_t obj)
{
  if(qz_is_cell(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell) qz_set_refcount(cell, qz_refcount(cell) + 1);
  }
  return obj;
}

qz_obj_t qz_unref(qz_obj_t obj)
{
  if(!qz_is_cell(obj))
    return obj;

  qz_cell_t* cell = qz_to_cell(obj);
  if(!cell)
    return obj;

  size_t refcount = qz_refcount(cell);
  refcount--;
  qz_set_refcount(cell, refcount);

  if(refcount)
    return obj;

  if(qz_type(cell) == QZ_CT_PAIR || qz_type(cell) == QZ_CT_FUN) {
    qz_unref(cell->value.pair.first);
    qz_unref(cell->value.pair.rest);
  }
  else if(qz_type(cell) == QZ_CT_STRING) {
    /* nothing to do */
  }
  else if(qz_type(cell) == QZ_CT_VECTOR) {
    qz_obj_t* data = QZ_CELL_DATA(cell, qz_obj_t);
    for(size_t i = 0; i < cell->value.array.size; i++)
      qz_unref(data[i]);
  }
  else if(qz_type(cell) == QZ_CT_BYTEVECTOR) {
    /* nothing to do */
  }
  else if(qz_type(cell) == QZ_CT_HASH) {
    qz_pair_t* data = QZ_CELL_DATA(cell, qz_pair_t);
    for(size_t i = 0; i < cell->value.array.capacity; i++) {
      qz_unref(data[i].first);
      qz_unref(data[i].rest);
    }
  }
  else if(qz_type(cell) == QZ_CT_REAL) {
    /* nothing to do */
  }
  else {
    assert(0); /* unknown cell type */
  }

  free(cell); /* I never liked that game */
  return QZ_NIL;
}

/* performs a bitwise comparison of two arrays
 * returns nonzero if equal */
static int compare_array(qz_cell_t* a, qz_cell_t* b, size_t elem_size)
{
  if(a->value.array.size != b->value.array.size)
    return 0;

  return memcmp(QZ_CELL_DATA(a, char), QZ_CELL_DATA(b, char), a->value.array.size*elem_size) == 0;
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

  /* straight compare failed for non-cell? not equal */
  if(a_tag != QZ_PT_CELL)
    return 0;

  qz_cell_t* a_cell = qz_to_cell(a);
  qz_cell_t* b_cell = qz_to_cell(b);

  qz_cell_type_t a_type = qz_type(a_cell);
  qz_cell_type_t b_type = qz_type(b_cell);

  /* different cell types? not equal */
  if(a_type != b_type)
    return 0;

  if(a_type == QZ_CT_PAIR || a_type == QZ_CT_FUN)
  {
    /* recusively compare pairs */
    return qz_equal(a_cell->value.pair.first, b_cell->value.pair.first)
      && qz_equal(b_cell->value.pair.rest, b_cell->value.pair.rest);
  }
  else if(a_type == QZ_CT_STRING)
  {
    /* bitwise compare strings */
    return compare_array(a_cell, b_cell, sizeof(char));
  }
  else if(a_type == QZ_CT_VECTOR)
  {
    /* recursively compare vectors */
    qz_array_t* a_vec = &a_cell->value.array;
    qz_array_t* b_vec = &b_cell->value.array;

    if(a_vec->size != b_vec->size)
      return 0;

    for(size_t i = 0; i < a_vec->size; i++) {
      if(!qz_equal(QZ_CELL_DATA(a_cell, qz_obj_t)[i], QZ_CELL_DATA(b_cell, qz_obj_t)[i]))
        return 0;
    }

    return 1;
  }
  else if(a_type == QZ_CT_BYTEVECTOR)
  {
    /* bitwise compare strings */
    return compare_array(a_cell, b_cell, sizeof(uint8_t));
  }
  else if(a_type == QZ_CT_REAL)
  {
    /* straight compare reals */
    return a_cell->value.real == b_cell->value.real;
  }
  else if(a_type == QZ_CT_HASH)
  {
    assert(0); /* NYI */
  }

  assert(0); /* unknown cell type */
}

