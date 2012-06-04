#include "quuz.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

qz_obj_t const QZ_NIL = { (size_t)NULL | QZ_PT_CELL };
qz_obj_t const QZ_TRUE = { (1 << 4) | QZ_PT_BOOL };
qz_obj_t const QZ_FALSE = { (0 << 4) | QZ_PT_BOOL };

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
    if(cell) return cell->type == type;
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
  return (qz_cell_t*)(obj.value & ~7);
}
qz_cfun_t qz_to_cfun(qz_obj_t obj) {
  assert(qz_is_cfun(obj));
  return (qz_cfun_t)(obj.value & ~7);
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

qz_obj_t qz_make_string(const char* str)
{
  size_t str_size = strlen(str);

  qz_cell_t* cell = (qz_cell_t*)malloc(sizeof(qz_cell_t) + str_size*sizeof(char));

  cell->type = QZ_CT_STRING;
  cell->refcount = 1;
  cell->value.array.size = str_size;
  cell->value.array.capacity = str_size;

  memcpy(QZ_CELL_DATA(cell, char), str, str_size);

  return qz_from_cell(cell);
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

qz_obj_t* qz_list_head(qz_obj_t obj)
{
  assert(qz_is_pair(obj));

  qz_cell_t* cell = qz_to_cell(obj);

  return &cell->value.pair.first;
}

qz_obj_t* qz_list_tail(qz_obj_t obj)
{
  assert(qz_is_pair(obj));

  qz_cell_t* cell = qz_to_cell(obj);

  while(!qz_is_nil(cell->value.pair.rest))
    cell = qz_to_cell(cell->value.pair.rest);

  return &cell->value.pair.first;
}

qz_obj_t* qz_vector_head(qz_obj_t obj)
{
  assert(qz_is_vector(obj));

  qz_cell_t* cell = qz_to_cell(obj);

  assert(cell->value.array.size != 0);

  return QZ_CELL_DATA(cell, qz_obj_t);
}

qz_obj_t* qz_vector_tail(qz_obj_t obj)
{
  assert(qz_is_vector(obj));

  qz_cell_t* cell = qz_to_cell(obj);

  assert(cell->value.array.size != 0);

  return QZ_CELL_DATA(cell, qz_obj_t) + (cell->value.array.size - 1);
}

static void inner_write(qz_state_t* st, qz_obj_t obj, int depth, FILE* fp, int* need_space);

static void inner_write_cell(qz_state_t* st, qz_cell_t* cell, int depth, FILE* fp, int* need_space)
{
  if(!cell) {
    if(*need_space) fputc(' ', fp);
    fputs("[nil]", fp);
    *need_space = 1;
    return;
  }

  if(cell->type == QZ_CT_PAIR)
  {
    qz_pair_t* pair = &cell->value.pair;

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
  else if(cell->type == QZ_CT_FUN)
  {
    if(*need_space) fputc(' ', fp);
    fputs("[fun]", fp); /* TODO How is scheme-defined function supposed to be written? */
    *need_space = 1;
  }
  else if(cell->type == QZ_CT_STRING)
  {
    if(*need_space) fputc(' ', fp);

    fputc('"', fp);
    for(size_t i = 0; i < cell->value.array.size; i++) {
      char c = QZ_CELL_DATA(cell, char)[i];
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
  else if(cell->type == QZ_CT_VECTOR)
  {
    if(*need_space) fputc(' ', fp);

    fputs("#(", fp);
    *need_space = 0;

    if(depth) {
      depth--;
      for(size_t i = 0; i < cell->value.array.size; i++)
        inner_write(st, QZ_CELL_DATA(cell, qz_obj_t)[i], depth, fp, need_space);
      depth++;
    }
    else {
      fputs("...", fp);
    }

    fputc(')', fp);
    *need_space = 1;
  }
  else if(cell->type == QZ_CT_BYTEVECTOR)
  {
    if(*need_space) fputc(' ', fp);

    fputs("#u8(", fp);
    *need_space = 0;

    for(size_t i = 0; i < cell->value.array.size; i++)
    {
      if(*need_space) fputc(' ', fp);

      fprintf(fp, "#x%02x", QZ_CELL_DATA(cell, uint8_t)[i]);
      *need_space = 1;
    }

    fputc(')', fp);
    *need_space = 1;
  }
  else if(cell->type == QZ_CT_HASH)
  {
    assert(0); /* implement for debugging purposes? */
  }
  else if(cell->type == QZ_CT_REAL)
  {
    // TODO make this readable by qz_read()
    if(*need_space) fputc(' ', fp);

    fprintf(fp, "%f", cell->value.real);
    *need_space = 1;
  }
  else
  {
    assert(0); /* unknown cell type */
  }
}

static void inner_write(qz_state_t* st, qz_obj_t obj, int depth, FILE* fp, int* need_space)
{
  if(qz_is_fixnum(obj))
  {
    if(*need_space) fputc(' ', fp);
    fprintf(fp, "%ld", qz_to_fixnum(obj));
    *need_space = 1;
  }
  else if(qz_is_cell(obj))
  {
    inner_write_cell(st, qz_to_cell(obj), depth, fp, need_space);
  }
  else if(qz_is_cfun(obj))
  {
    if(*need_space) fputc(' ', fp);
    fprintf(fp, "[cfun %p]", (void*)(size_t)qz_to_cfun(obj));
    *need_space = 1;
  }
  else if(qz_is_sym(obj))
  {
    if(*need_space) fputc(' ', fp);

    // translate identifier to string
    obj = qz_get_hash(st->sym_name, obj);

    // TODO make this readable by qz_read()
    assert(qz_is_string(obj));
    qz_cell_t* cell = qz_to_cell(obj);
    fprintf(fp, "%.*s", (int)cell->value.array.size, QZ_CELL_DATA(cell, char));

    *need_space = 1;
  }
  else if(qz_is_bool(obj))
  {
    if(*need_space) fputc(' ', fp);

    int b = qz_to_bool(obj);
    if(b)
      fputs("#t", fp);
    else
      fputs("#f", fp);

    *need_space = 1;
  }
  else if(qz_is_char(obj))
  {
    if(*need_space) fputc(' ', fp);

    char c = qz_to_char(obj);
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
  if(!qz_is_cell(obj))
    return;

  qz_cell_t* cell = qz_to_cell(obj);
  if(!cell)
    return;

  cell->refcount++;
}

void qz_unref(qz_obj_t obj)
{
  if(!qz_is_cell(obj))
    return;

  qz_cell_t* cell = qz_to_cell(obj);
  if(!cell)
    return;

  if(--cell->refcount)
    return;

  if(cell->type == QZ_CT_PAIR || cell->type == QZ_CT_FUN) {
    qz_unref(cell->value.pair.first);
    qz_unref(cell->value.pair.rest);
  }
  else if(cell->type == QZ_CT_STRING) {
    /* nothing to do */
  }
  else if(cell->type == QZ_CT_VECTOR) {
    qz_obj_t* data = QZ_CELL_DATA(cell, qz_obj_t);
    for(size_t i = 0; i < cell->value.array.size; i++)
      qz_unref(data[i]);
  }
  else if(cell->type == QZ_CT_BYTEVECTOR) {
    /* nothing to do */
  }
  else if(cell->type == QZ_CT_HASH) {
    qz_pair_t* data = QZ_CELL_DATA(cell, qz_pair_t);
    for(size_t i = 0; i < cell->value.array.capacity; i++) {
      qz_unref(data[i].first);
      qz_unref(data[i].rest);
    }
  }
  else if(cell->type == QZ_CT_REAL) {
    /* nothing to do */
  }
  else {
    assert(0); /* unknown cell type */
  }

  free(cell); /* I never liked that game */
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

  /* different cell types? not equal */
  if(a_cell->type != b_cell->type)
    return 0;

  if(a_cell->type == QZ_CT_PAIR || a_cell->type == QZ_CT_FUN)
  {
    /* recusively compare pairs */
    return qz_equal(a_cell->value.pair.first, b_cell->value.pair.first)
      && qz_equal(b_cell->value.pair.rest, b_cell->value.pair.rest);
  }
  else if(a_cell->type == QZ_CT_STRING)
  {
    /* bitwise compare strings */
    return compare_array(a_cell, b_cell, sizeof(char));
  }
  else if(a_cell->type == QZ_CT_VECTOR)
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
  else if(a_cell->type == QZ_CT_BYTEVECTOR)
  {
    /* bitwise compare strings */
    return compare_array(a_cell, b_cell, sizeof(uint8_t));
  }
  else if(a_cell->type == QZ_CT_REAL)
  {
    /* straight compare reals */
    return a_cell->value.real == b_cell->value.real;
  }
  else if(a_cell->type == QZ_CT_HASH)
  {
    assert(0); /* NYI */
  }

  assert(0); /* unknown cell type */
}

