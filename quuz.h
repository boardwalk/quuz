#ifndef QUUZ_QUUZ_H
#define QUUZ_QUUZ_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*  000 even fixnum (value is ptr >> 2)
 *  001 short immediate (see below)
 *  010 cell
 *  011 string
 *  100 odd fixnum (value is ptr >> 2)
 *  101 identifier
 *  110 vector
 *  111 bytevector
 * 0001 boolean (value is ptr >> 4)
 * 1001 character (value is ptr >> 4) */

typedef enum {
  QZ_PT_EVEN_FIXNUM = 0, /* TODO signed? */
  QZ_PT_SHORT_IMM = 1,
  QZ_PT_CELL = 2,
  QZ_PT_STRING = 3,
  QZ_PT_ODD_FIXNUM = 4,
  QZ_PT_IDENTIFIER = 5,
  QZ_PT_VECTOR = 6,
  QZ_PT_BYTEVECTOR = 7,
  QZ_PT_BOOL = 1,
  QZ_PT_CHAR = 9
} qz_pointer_tag_t;

typedef enum {
  QZ_CT_PAIR,
  QZ_CT_REAL
} qz_cell_type_t;

typedef struct { size_t value; } qz_obj_t;

typedef struct qz_pair {
  qz_obj_t first;
  qz_obj_t rest;
} qz_pair_t;

typedef struct qz_string {
  size_t size;
  char data[];
} qz_string_t;

typedef struct qz_vector {
  size_t size;
  qz_obj_t data[];
} qz_vector_t;

typedef struct qz_bytevector {
  size_t size;
  uint8_t data[];
} qz_bytevector_t;

typedef struct qz_cell {
  qz_cell_type_t type; /* possibly could pack more data in here */
  union {
    qz_pair_t pair;
    double real;
  };
} qz_cell_t;

/* quuz-cell.h */
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
int qz_is_real(qz_obj_t);

size_t qz_to_fixnum(qz_obj_t);
qz_cell_t* qz_to_cell(qz_obj_t);
qz_string_t* qz_to_string(qz_obj_t);
qz_string_t* qz_to_identifier(qz_obj_t);
qz_vector_t* qz_to_vector(qz_obj_t);
qz_bytevector_t* qz_to_bytevector(qz_obj_t);
int qz_to_bool(qz_obj_t);
char qz_to_char(qz_obj_t);
qz_pair_t* qz_to_pair(qz_obj_t);
double qz_to_real(qz_obj_t);

qz_obj_t qz_from_fixnum(size_t);
qz_obj_t qz_from_cell(qz_cell_t*);
qz_obj_t qz_from_string(qz_string_t*);
qz_obj_t qz_from_identifier(qz_string_t*);
qz_obj_t qz_from_vector(qz_vector_t*);
qz_obj_t qz_from_bytevector(qz_bytevector_t*);
qz_obj_t qz_from_bool(int);
qz_obj_t qz_from_char(char);

void qz_write(qz_obj_t obj, int depth, FILE* fp);
void qz_destroy(qz_obj_t obj);

/* quuz-read.c */
qz_obj_t qz_read(FILE*);

#endif /* QUUZ_QUUZ_H */
