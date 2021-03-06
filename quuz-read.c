#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

#define YY_INPUT(buf, result, max_size)            \
  {                                                \
    int yyc = fgetc(g_fp);                         \
    result = (EOF == yyc) ? 0 : (*(buf) = yyc, 1); \
    yyprintf((stderr, "<%c>", yyc));               \
  }

static qz_state_t* g_st = NULL;
static FILE* g_fp = NULL;
static qz_obj_t g_stack;

static qz_obj_t make_array(qz_cell_type_t type, size_t elem_size)
{
  qz_cell_t* cell = qz_make_cell(type, INITIAL_CAPACITY*elem_size);

  cell->value.array.size = 0;
  cell->value.array.capacity = INITIAL_CAPACITY;

  return qz_from_cell(cell);
}

/* double the capacity of an array */
static qz_cell_t* grow_array(qz_cell_t* cell, size_t elem_size)
{
  /* if someone else has a reference to this array, you're gonna have a bad time */
  assert(qz_refcount(cell) == 1);

  /* double capacity */
  size_t new_capacity = cell->value.array.capacity * 2;

  /* make copy of array */
  qz_cell_t* new_cell = (qz_cell_t*)malloc(sizeof(qz_cell_t) + new_capacity*elem_size);
  assert(((size_t)new_cell & 7) == 0);

  /* copy info */
  new_cell->info = cell->info;

  /* init array */
  new_cell->value.array.size = cell->value.array.size;
  new_cell->value.array.capacity = new_capacity;

  /* copy data */
  memcpy(QZ_CELL_DATA(new_cell, char), QZ_CELL_DATA(cell, char), cell->value.array.size*elem_size);

  /* cleanup */
  free(cell);

  return new_cell;
}

static void concat_string(char c)
{
  /*printf("concat_string(%c)\n", c);*/

  qz_obj_t* obj = qz_vector_tail_ptr(g_stack);
  qz_cell_t* cell = qz_to_cell(*obj);

  /* resize if necessary */
  if(cell->value.array.size == cell->value.array.capacity) {
    cell = grow_array(cell, sizeof(char));
    *obj = qz_from_cell(cell);
  }

  /* append character */
  QZ_CELL_DATA(cell, char)[cell->value.array.size++] = c;
}

static void concat_bytevector(int b)
{
  /*printf("concat_bytevector(%d)\n", b);*/

  qz_obj_t* obj = qz_vector_tail_ptr(g_stack);
  qz_cell_t* cell = qz_to_cell(*obj);

  /* resize if necessary */
  if(cell->value.array.size == cell->value.array.capacity) {
    cell = grow_array(cell, sizeof(uint8_t));
    *obj = qz_from_cell(cell);
  }

  /* append byte */
  QZ_CELL_DATA(cell, uint8_t)[cell->value.array.size++] = b;
}

static void append(qz_obj_t value_obj)
{
  qz_obj_t* obj = qz_vector_tail_ptr(g_stack);

  /*printf("append(%lu)\n", value_obj.value);*/

  if(qz_is_null(*obj))
  {
    /* parsing is the only place a null is promoted to a pair */
    *obj = qz_make_pair(value_obj, QZ_NULL);
  }
  else if(qz_is_pair(*obj))
  {
    qz_pair_t* pair = qz_to_pair(*obj);

    /* walk to the end of the list */
    while(!qz_is_null(pair->rest))
      pair = qz_to_pair(pair->rest);

    /* wrap in another cell and append */
    pair->rest = qz_make_pair(value_obj, QZ_NULL);
  }
  else if(qz_is_vector(*obj))
  {
    qz_cell_t* cell = qz_to_cell(*obj);

    /* resize if necessary */
    if(cell->value.array.size == cell->value.array.capacity) {
      cell = grow_array(cell, sizeof(qz_obj_t));
      *obj = qz_from_cell(cell);
    }

    /* append character */
    QZ_CELL_DATA(cell, qz_obj_t)[cell->value.array.size++] = value_obj;
  }
  else
  {
    assert(0); /* attempt to append cell to object of wrong type */
  }

  /*printf("  stack obj (after): "); qz_write(*obj, -1, stdout); fputc('\n', stdout);*/
}

/* push an object onto the stack */
static void push(qz_obj_t obj)
{
  /*printf("push()\n");*/

  qz_cell_t* stack_cell = qz_to_cell(g_stack);

  /* resize if necessary */
  if(stack_cell->value.array.size == stack_cell->value.array.capacity) {
    stack_cell = grow_array(stack_cell, sizeof(qz_obj_t));
    g_stack = qz_from_cell(stack_cell);
  }
  
  /* append object */
  QZ_CELL_DATA(stack_cell, qz_obj_t)[stack_cell->value.array.size++] = obj;
}

/* pop an object from the stack, appending it to the container at the new top of the stack */
static void pop(void)
{
  /*printf("pop()\n");*/

  qz_cell_t* stack_cell = qz_to_cell(g_stack);

  assert(stack_cell->value.array.size > 1); /* never pop the root element */

  qz_obj_t obj = QZ_CELL_DATA(stack_cell, qz_obj_t)[--stack_cell->value.array.size];

  /* append a null to strings */
  if(qz_is_string(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell->value.array.size == cell->value.array.capacity) {
      cell = grow_array(cell, sizeof(char));
      obj = qz_from_cell(cell);
    }
    QZ_CELL_DATA(cell, char)[cell->value.array.size] = '\0';
  }

  append(obj);
}

/* pop a string from the stack, appending the matching symbol to the container at the top of the stack */
static void pop_sym(void)
{
  /*printf("pop_sym()\n");*/

  qz_cell_t* stack_cell = qz_to_cell(g_stack);

  assert(stack_cell->value.array.size > 1); /* never pop the root element */

  qz_obj_t obj = QZ_CELL_DATA(stack_cell, qz_obj_t)[--stack_cell->value.array.size];

  /* append a null to strings */
  if(qz_is_string(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell->value.array.size == cell->value.array.capacity) {
      cell = grow_array(cell, sizeof(char));
      obj = qz_from_cell(cell);
    }
    QZ_CELL_DATA(cell, char)[cell->value.array.size] = '\0';
  }

  append(qz_make_sym(g_st, obj));
}

/* (a b c) -> (a b . c) */
static void elide_pair(void)
{
  /*printf("elide_pair();\n");*/

  qz_cell_t* stack_cell = qz_to_cell(g_stack);

  qz_obj_t prev;
  qz_obj_t curr = QZ_CELL_DATA(stack_cell, qz_obj_t)[stack_cell->value.array.size - 1];

  for(;;) {
    qz_pair_t* pair = qz_to_pair(curr);

    if(qz_is_null(pair->rest)) {
      qz_to_pair(prev)->rest = pair->first;
      pair->first = QZ_NULL;
      qz_obliterate(g_st, curr);
      break;
    }

    prev = curr;
    curr = pair->rest;
  }
}

/* push a pair onto the stack */
static void push_pair(void)
{
  /*printf("push_pair()\n");*/

  push(QZ_NULL);
}

/* push a vector onto the stack */
static void push_vector(void)
{
  /*printf("push_vector()\n");*/

  push(make_array(QZ_CT_VECTOR, sizeof(qz_obj_t)));
}

/* push a bytevector onto the stack */
static void push_bytevector(void)
{
  /*printf("push_bytevector()\n");*/

  push(make_array(QZ_CT_BYTEVECTOR, sizeof(uint8_t)));
}

/* push a string onto the stack */
static void push_string(void)
{
  /*printf("push_string()\n");*/

  push(make_array(QZ_CT_STRING, sizeof(char)));
}

/* append a char value to the container at the top of the stack */
static void append_char(char c)
{
  /*printf("append_char(%c (%d))\n", c, c);*/

  append(qz_from_char(c));
}

/* append a number value to the container at the top of the stack */
static void append_number(intptr_t i)
{
  /*printf("append_number(%ld)\n", i);*/

  append(qz_from_fixnum(i));
}

/* append a boolean value to the container at the top of the stack */
static void append_bool(int b)
{
  /*printf("append_boolean(%d)\n", b);*/

  append(qz_from_bool(b));
}

/* append an identifier constructed from a C-style string to container at the top of the stack */
static void append_sym(const char* s)
{
  /*printf("push_identifier_c(%s)\n", s);*/

  push_string();
  while(*s)
    concat_string(*s++);
  pop_sym();
}

//#define YY_DEBUG
#define YYSTYPE intptr_t
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "parser.c"
#pragma GCC diagnostic pop

void qz_discard_hashbang(FILE* fp)
{
  g_fp = fp;
  yyparsefrom(yy_hashBang);
  g_fp = NULL;
}

qz_obj_t qz_read(qz_state_t* st, FILE* fp)
{
  /* avoid unused function warning */
  (void)yyAccept;

  g_st = st;
  g_fp = fp;

  /* setup stack with empty list */
  g_stack = make_array(QZ_CT_VECTOR, sizeof(qz_obj_t));
  push(QZ_NULL);

  /* parse file */
  yyparse();

  /* grab result */
  qz_obj_t stack_top = qz_vector_head(g_stack);
  qz_obj_t result = QZ_NONE;
  if(qz_is_cell(stack_top) && !qz_is_null(stack_top))
    result = qz_ref(st, qz_list_head(stack_top));

  /* cleanup */
  qz_unref(st, g_stack);
  g_stack = QZ_NONE;
  g_st = NULL;
  g_fp = NULL;

  return result;
}
