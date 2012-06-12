#ifndef QUUZ_QUUZ_H
#define QUUZ_QUUZ_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define QZ_ROOT_BUFFER_CAPACITY 16
#define QZ_SAFETY_BUFFER_CAPACITY 16
#define QZ_CELL_DATA(c, t) ((t*)((char*)(c) + sizeof(qz_cell_t)))

/*   000 even fixnum (value is << 2)
 *   001 short immediate (see below)
 *   010 cell
 *   011 c function
 *   100 odd fixnum (value is << 2)
 * 00001 symbol (value is << 5)
 * 01001 boolean (value is << 5)
 * 10001 character (value is << 5) */

typedef enum {
  QZ_PT_EVEN_FIXNUM = 0,
  QZ_PT_SHORT_IMM = 1,
  QZ_PT_CELL = 2,
  QZ_PT_CFUN = 3,
  QZ_PT_ODD_FIXNUM = 4,
  QZ_PT_SYM = 1,
  QZ_PT_BOOL = 9,
  QZ_PT_CHAR = 17
} qz_pointer_tag_t;

typedef enum {
  QZ_CT_PAIR, /* qz_pair_t */
  QZ_CT_FUN, /* qz_pair_t, environment in first, formals & body in rest */
  QZ_CT_STRING, /* qz_array_t with char elements follows qz_cell_t */
  QZ_CT_VECTOR, /* qz_array_t with qz_obj_t elements */
  QZ_CT_BYTEVECTOR, /* qz_array_t with uint8_t elements */
  QZ_CT_HASH, /* qz_array_t with qz_pair_t elements */
  QZ_CT_REAL
  /* 7 values, 3 bits */
} qz_cell_type_t;

typedef enum {
  QZ_CC_BLACK, /* in use or free */
  QZ_CC_GRAY, /* possible member of cycle */
  QZ_CC_WHITE, /* member of garbage cycle */
  QZ_CC_PURPLE, /* possible root of cycle */
  /* 4 values, 2 bits */
} qz_cell_color_t;

typedef struct { size_t value; } qz_obj_t;

typedef struct qz_pair {
  qz_obj_t first;
  qz_obj_t rest;
} qz_pair_t;

typedef struct qz_array {
  size_t size; /* in elements, not bytes */
  size_t capacity; /* ditto */
  /* data follows */
} qz_array_t;

typedef struct qz_cell {
  // contains four fields, lsb to msb
  // refcount, sizeof(size_t)*CHAR_BIT - 6 bits
  // type, 3 bits, qz_cell_type_t
  // color, 2 bits, qz_cell_color_t
  // buffered, 1 bit
  size_t info;
  union {
    qz_pair_t pair;
    qz_array_t array;
    double real;
  } value;
  /* don't put anything beyond the union. qz_array_t is variable in size */
} qz_cell_t;

typedef struct qz_state {
  /* array of possible roots */
  size_t root_buffer_size;
  qz_cell_t* root_buffer[QZ_ROOT_BUFFER_CAPACITY];

  /* state to restore when an error occurs */
  jmp_buf* error_handler;

  /* message to print after catching an error */
  const char* error_msg;

  /* array of objects to unref if a peval() fails */
  size_t safety_buffer_size;
  qz_obj_t safety_buffer[QZ_SAFETY_BUFFER_CAPACITY];

  /* variables bindings
   * a list of a list of hashes
   * the outer list is a stack of environment for currently executing functions
   * the inner list is a stack of scopes for a particular function
   * each hash maps symbols to values */
  qz_obj_t env;

  /* a hash mapping names to symbols */
  qz_obj_t name_sym;

  /* a hash mapping symbols to names */
  qz_obj_t sym_name;

  /* next number to assign to a symbol */
  size_t next_sym;

  /* "begin" sym, used in "lambda" and "define" */
  qz_obj_t begin_sym;

  /* "else" sym, used in "cond" */
  qz_obj_t else_sym;

  /* "=>" sym, used in "cond" */
  qz_obj_t arrow_sym;
} qz_state_t;

typedef qz_obj_t (*qz_cfun_t)(qz_state_t* st, qz_obj_t args);

typedef struct qz_named_cfun {
  qz_cfun_t cfun;
  const char* name;
} qz_named_cfun_t;

/******************************************************************************
 * quuz-object.c
 ******************************************************************************/
extern qz_obj_t const QZ_NIL;
extern qz_obj_t const QZ_TRUE;
extern qz_obj_t const QZ_FALSE;

int qz_is_nil(qz_obj_t);
int qz_is_fixnum(qz_obj_t);
int qz_is_cell(qz_obj_t);
int qz_is_cfun(qz_obj_t);
int qz_is_sym(qz_obj_t);
int qz_is_bool(qz_obj_t);
int qz_is_char(qz_obj_t);
int qz_is_pair(qz_obj_t);
int qz_is_fun(qz_obj_t);
int qz_is_string(qz_obj_t);
int qz_is_vector(qz_obj_t);
int qz_is_bytevector(qz_obj_t);
int qz_is_hash(qz_obj_t);
int qz_is_real(qz_obj_t);

intptr_t qz_to_fixnum(qz_obj_t);
qz_cell_t* qz_to_cell(qz_obj_t);
qz_cfun_t qz_to_cfun(qz_obj_t);
size_t qz_to_sym(qz_obj_t);
int qz_to_bool(qz_obj_t);
char qz_to_char(qz_obj_t);
qz_pair_t* qz_to_pair(qz_obj_t);
qz_pair_t* qz_to_fun(qz_obj_t);
double qz_to_real(qz_obj_t);

qz_obj_t qz_from_fixnum(intptr_t);
qz_obj_t qz_from_cell(qz_cell_t*);
qz_obj_t qz_from_cfun(qz_cfun_t);
qz_obj_t qz_from_bool(int);
qz_obj_t qz_from_char(char);

size_t qz_refcount(qz_cell_t*);
qz_cell_type_t qz_type(qz_cell_t*);
qz_cell_color_t qz_color(qz_cell_t*);
size_t qz_buffered(qz_cell_t*);

void qz_set_refcount(qz_cell_t* cell, size_t rc);
void qz_set_type(qz_cell_t* cell, qz_cell_type_t ct);
void qz_set_color(qz_cell_t* cell, qz_cell_color_t cc);
void qz_set_buffered(qz_cell_t* cell, size_t bu);

qz_cell_t* qz_make_cell(qz_cell_type_t type, size_t extra_size);
qz_obj_t qz_make_string(const char* str);
qz_obj_t qz_make_pair(qz_obj_t first, qz_obj_t rest);
qz_obj_t qz_make_sym(qz_state_t* st, qz_obj_t name);

/* returns the first member of a pair
 * qz_is_pair(obj) must be true */
qz_obj_t qz_first(qz_obj_t);

/* returns the rest member of a pair
 * qz_is_pair(obj) must be true */
qz_obj_t qz_rest(qz_obj_t);

qz_obj_t qz_required_arg(qz_state_t* st, qz_obj_t* obj);
qz_obj_t qz_optional_arg(qz_state_t* st, qz_obj_t* obj);

qz_obj_t* qz_list_head_ptr(qz_obj_t obj);
qz_obj_t* qz_list_tail_ptr(qz_obj_t obj);

qz_obj_t qz_list_head(qz_obj_t obj);
qz_obj_t qz_list_tail(qz_obj_t obj);

qz_obj_t* qz_vector_head_ptr(qz_obj_t obj);
qz_obj_t* qz_vector_tail_ptr(qz_obj_t obj);

qz_obj_t qz_vector_head(qz_obj_t obj);
qz_obj_t qz_vector_tail(qz_obj_t obj);

int qz_eqv(qz_obj_t a, qz_obj_t b);
int qz_equal(qz_obj_t a, qz_obj_t b);

/******************************************************************************
 * quuz-hash.c
 ******************************************************************************/
qz_obj_t qz_make_hash(void);
qz_obj_t qz_get_hash(qz_state_t* st, qz_obj_t obj, qz_obj_t key);
void qz_set_hash(qz_state_t* st, qz_obj_t* obj, qz_obj_t key, qz_obj_t value);

/******************************************************************************
 * quuz-state.c
 ******************************************************************************/
qz_state_t* qz_alloc(void);
void qz_free(qz_state_t* st);
qz_obj_t qz_peval(qz_state_t* st, qz_obj_t obj);
qz_obj_t qz_eval(qz_state_t* st, qz_obj_t obj);

/* throw an error. doesn't return
 * msg must be statically allocated */
qz_obj_t qz_error(qz_state_t* st, const char* msg);

/* push an object onto the safety buffer
 * objects in this buffer will be unref'd if a peval at a higher level catches an error */
void qz_push_safety(qz_state_t* st, qz_obj_t obj);

/* pop nobj objects from the safety buffer */
void qz_pop_safety(qz_state_t* st, size_t nobj);

/******************************************************************************
 * quuz-read.c
 ******************************************************************************/
qz_obj_t qz_read(qz_state_t* st, FILE* fp);

/******************************************************************************
 * quuz-write.c
 ******************************************************************************/
void qz_write(qz_state_t* st, qz_obj_t obj, int depth, FILE* fp);

/******************************************************************************
 * quuz-collector.c
 ******************************************************************************/
qz_obj_t qz_ref(qz_state_t* st, qz_obj_t obj);
void qz_unref(qz_state_t* st, qz_obj_t obj);
void qz_collect(qz_state_t* st);

#endif /* QUUZ_QUUZ_H */
