#ifndef QUUZ_QUUZ_H
#define QUUZ_QUUZ_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*   000 even fixnum (value is ptr >> 2)
 *   001 short immediate (see below)
 *   010 cell
 *   011 string
 *   100 odd fixnum (value is ptr >> 2)
 *   110 vector
 *   111 bytevector
 * 00001 identifier (value is ptr >> 5)
 * 01001 boolean (value is ptr >> 5)
 * 10001 character (value is ptr >> 5) */

#define QZ_ARRAY_DATA(a, t) ((t*)((char*)(a) + sizeof(qz_array_t)))
#define QZ_HASH_DATA(a) ((qz_pair_t*)((char*)(a) + sizeof(qz_hash_t)))

typedef enum {
  QZ_PT_EVEN_FIXNUM = 0,
  QZ_PT_SHORT_IMM = 1,
  QZ_PT_CELL = 2,
  QZ_PT_STRING = 3,
  QZ_PT_ODD_FIXNUM = 4,
  QZ_PT_VECTOR = 5,
  QZ_PT_BYTEVECTOR = 6,
  QZ_PT_IDENTIFIER = 1,
  QZ_PT_BOOL = 9,
  QZ_PT_CHAR = 17
} qz_pointer_tag_t;

typedef enum {
  QZ_CT_PAIR,
  QZ_CT_HASH,
  QZ_CT_CFN,
  QZ_CT_REAL
} qz_cell_type_t;

typedef struct { size_t value; } qz_obj_t;

typedef struct qz_state {
  /* variables bindings
   * a stack-style list with hashes */
  qz_obj_t env;
  /* a hash mapping strings to identifiers */
  qz_obj_t str_iden;
  /* a hash mapping identifiers to strings */
  qz_obj_t iden_str;
  /* next number to assign to an identifier */
  size_t next_iden;
  /* state to restore when an error occurs */
  jmp_buf error_handler;
} qz_state_t;

typedef struct qz_array {
  size_t size; /* in elements, not bytes */
  uint16_t refcount;
  /* data follows */
} qz_array_t;

typedef struct qz_pair {
  qz_obj_t first;
  qz_obj_t rest;
} qz_pair_t;

typedef struct qz_hash {
  size_t size; /* in elements, not bytes */
  size_t capacity; /* ditto */
  /* qz_pair_t data follows */
} qz_hash_t;

typedef qz_obj_t (*qz_cfn_t)(qz_state_t* st, qz_obj_t args);

typedef struct qz_cell {
  uint16_t type; /* qz_cell_type_t */
  uint16_t refcount;
  union {
    qz_pair_t pair;
    qz_hash_t hash;
    qz_cfn_t cfn;
    double real;
  } value;
  /* don't put anything beyond the union. qz_hash_t is variable in size */
} qz_cell_t;

/* quuz-object.c */
extern qz_obj_t const QZ_NIL;
extern qz_obj_t const QZ_TRUE;
extern qz_obj_t const QZ_FALSE;

int qz_is_nil(qz_obj_t); /* TODO this checks for a null pointer to a cell, not a pair with null pointers (empty list) confusing? */
int qz_is_fixnum(qz_obj_t);
int qz_is_cell(qz_obj_t);
int qz_is_string(qz_obj_t);
int qz_is_identifier(qz_obj_t);
int qz_is_vector(qz_obj_t);
int qz_is_bytevector(qz_obj_t);
int qz_is_bool(qz_obj_t);
int qz_is_char(qz_obj_t);
int qz_is_pair(qz_obj_t);
int qz_is_hash(qz_obj_t);
int qz_is_cfn(qz_obj_t);
int qz_is_real(qz_obj_t);

intptr_t qz_to_fixnum(qz_obj_t);
qz_cell_t* qz_to_cell(qz_obj_t);
qz_array_t* qz_to_string(qz_obj_t);
qz_array_t* qz_to_vector(qz_obj_t);
qz_array_t* qz_to_bytevector(qz_obj_t);
int qz_to_bool(qz_obj_t);
char qz_to_char(qz_obj_t);
qz_pair_t* qz_to_pair(qz_obj_t);
qz_hash_t* qz_to_hash(qz_obj_t);
qz_cfn_t qz_to_cfn(qz_obj_t);
double qz_to_real(qz_obj_t);

qz_obj_t qz_from_fixnum(intptr_t);
qz_obj_t qz_from_cell(qz_cell_t*);
qz_obj_t qz_from_string(qz_array_t*);
qz_obj_t qz_from_identifier(qz_array_t*);
qz_obj_t qz_from_vector(qz_array_t*);
qz_obj_t qz_from_bytevector(qz_array_t*);
qz_obj_t qz_from_bool(int);
qz_obj_t qz_from_char(char);

qz_obj_t qz_make_string(const char* str);
qz_obj_t qz_make_pair(qz_obj_t first, qz_obj_t rest);
qz_obj_t qz_make_iden(qz_state_t* st, qz_obj_t name);
qz_obj_t qz_make_cfn(qz_cfn_t cfn);

qz_obj_t* qz_list_head(qz_obj_t obj);
qz_obj_t* qz_list_tail(qz_obj_t obj);

void qz_write(qz_state_t* st, qz_obj_t obj, int depth, FILE* fp);
void qz_ref(qz_obj_t obj);
void qz_unref(qz_obj_t obj);
int qz_equal(qz_obj_t a, qz_obj_t b);

/* quuz-hash.c */
qz_obj_t qz_make_hash();
qz_obj_t qz_get_hash(qz_obj_t obj, qz_obj_t key);
void qz_set_hash(qz_obj_t* obj, qz_obj_t key, qz_obj_t value);

/* quuz-state.c */
qz_state_t* qz_alloc();
void qz_free(qz_state_t* st);
qz_obj_t qz_peval(qz_state_t* st, qz_obj_t obj);
qz_obj_t qz_eval(qz_state_t* st, qz_obj_t obj);
qz_obj_t qz_error(qz_state_t* st, const char* msg, qz_obj_t context);

/* quuz-read.c */
qz_obj_t qz_read(qz_state_t* st, FILE* fp);


#endif /* QUUZ_QUUZ_H */
