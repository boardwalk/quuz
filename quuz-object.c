#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

const qz_obj_t QZ_NULL = { (size_t)NULL | QZ_PT_CELL };
const qz_obj_t QZ_TRUE = { (1 << 5) | QZ_PT_BOOL };
const qz_obj_t QZ_FALSE = { (0 << 5) | QZ_PT_BOOL };
const qz_obj_t QZ_NONE = { QZ_PT_NONE };

/* qz_is_<type> */
static int cell_of_type(qz_obj_t obj, qz_cell_type_t type) {
  if(qz_is_cell(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell) return qz_type(cell) == type;
  }
  return 0;
}
int qz_is_null(qz_obj_t obj) {
  return obj.value == QZ_NULL.value;
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
int qz_is_none(qz_obj_t obj) {
  return obj.value == QZ_NONE.value;
}
int qz_is_pair(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_PAIR);
}
int qz_is_fun(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_FUN);
}
int qz_is_promise(qz_obj_t obj) {
  return cell_of_type(obj, QZ_CT_PROMISE);
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
qz_pair_t* qz_to_fun(qz_obj_t obj) {
  assert(qz_is_fun(obj));
  return &qz_to_cell(obj)->value.pair;
}
qz_pair_t* qz_to_promise(qz_obj_t obj) {
  assert(qz_is_promise(obj));
  return &qz_to_cell(obj)->value.pair;
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

/* cell->info accessors */
#define REFCOUNT_BITS (sizeof(size_t)*CHAR_BIT - TYPE_BITS - COLOR_BITS - BUFFERED_BITS)
#define TYPE_BITS 3
#define COLOR_BITS 2
#define BUFFERED_BITS 1

static size_t get_bits(size_t bitfield, size_t pos, size_t len) {
  size_t mask = ~(size_t)0 >> (sizeof(size_t)*CHAR_BIT - len);
  return (bitfield >> pos) & mask;
}
static size_t set_bits(size_t bitfield, size_t pos, size_t len, size_t value) {
  size_t mask = ~(size_t)0 >> (sizeof(size_t)*CHAR_BIT - len);
  return (bitfield & ~(mask << pos)) | (value << pos);
}
size_t qz_refcount(qz_cell_t* cell) {
  return get_bits(cell->info, 0, REFCOUNT_BITS);
}
qz_cell_type_t qz_type(qz_cell_t* cell) {
  return get_bits(cell->info, REFCOUNT_BITS, TYPE_BITS);
}
qz_cell_color_t qz_color(qz_cell_t* cell) {
  return get_bits(cell->info, REFCOUNT_BITS + TYPE_BITS, COLOR_BITS);
}
size_t qz_buffered(qz_cell_t* cell) {
  return get_bits(cell->info, REFCOUNT_BITS + TYPE_BITS + COLOR_BITS, BUFFERED_BITS);
}
void qz_set_refcount(qz_cell_t* cell, size_t rc) {
  cell->info = set_bits(cell->info, 0, REFCOUNT_BITS, rc);
}
void qz_set_type(qz_cell_t* cell, qz_cell_type_t ct) {
  cell->info = set_bits(cell->info, REFCOUNT_BITS, TYPE_BITS, ct);
}
void qz_set_color(qz_cell_t* cell, qz_cell_color_t cc) {
  cell->info = set_bits(cell->info, REFCOUNT_BITS + TYPE_BITS, COLOR_BITS, cc);
}
void qz_set_buffered(qz_cell_t* cell, size_t bu) {
  cell->info = set_bits(cell->info, REFCOUNT_BITS + TYPE_BITS + COLOR_BITS, BUFFERED_BITS, bu);
}

qz_cell_t* qz_make_cell(qz_cell_type_t type, size_t extra_size)
{
  qz_cell_t* cell = (qz_cell_t*)malloc(sizeof(qz_cell_t) + extra_size);
  cell->info = 1 /*refcount*/ | ((size_t)type << REFCOUNT_BITS);
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
  /* find existing symbol */
  qz_obj_t* slot = qz_hash_get(st, st->name_sym, name);
  if(slot) {
    qz_unref(st, name); /* (1) -1 */
    return *slot;
  }

  /* create new symbol */
  qz_obj_t sym = (qz_obj_t) { (st->next_sym++ << 5) | QZ_PT_SYM };
  qz_ref(st, name); /* (1) +1 -2 */
  qz_hash_set(st, &st->name_sym, name, sym);
  qz_hash_set(st, &st->sym_name, sym, name);
  return sym;
}

qz_obj_t qz_first(qz_obj_t obj) {
  return qz_to_cell(obj)->value.pair.first;
}

qz_obj_t qz_rest(qz_obj_t obj) {
  return qz_to_cell(obj)->value.pair.rest;
}

qz_obj_t qz_required_arg(qz_state_t* st, qz_obj_t* obj)
{
  if(!qz_is_pair(*obj))
    qz_error(st, "expected list");
  qz_pair_t* pair = qz_to_pair(*obj);
  *obj = pair->rest;
  return pair->first;
}

qz_obj_t qz_optional_arg(qz_state_t* st, qz_obj_t* obj)
{
  if(!qz_is_pair(*obj))
    return QZ_NONE;
  qz_pair_t* pair = qz_to_pair(*obj);
  *obj = pair->rest;
  return pair->first;
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
  while(!qz_is_null(cell->value.pair.rest))
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

/* scheme's eq? procedure */
int qz_eq(qz_obj_t a, qz_obj_t b)
{
  return a.value == b.value;
}

/* scheme's eqv? procedure */
int qz_eqv(qz_obj_t a, qz_obj_t b)
{
  return a.value == b.value;
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

  if(a_type == QZ_CT_PAIR || a_type == QZ_CT_FUN || a_type == QZ_CT_PROMISE)
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

