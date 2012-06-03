#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define YY_INPUT(buf, result, max_size)            \
  {                                                \
    int yyc = fgetc(g_fp);                         \
    result = (EOF == yyc) ? 0 : (*(buf) = yyc, 1); \
    yyprintf((stderr, "<%c>", yyc));               \
  }

static qz_state_t* g_st = NULL;
static FILE* g_fp = NULL;
static qz_array_t* g_stack = NULL;
static int g_dotted_datum = 0;

static int is_zero_or_power_of_two(size_t v)
{
  return !(v & (v - 1));
}

static qz_array_t* alloc_array(size_t elem_size)
{
  qz_array_t* arr = (qz_array_t*)malloc(sizeof(qz_array_t) + 16*elem_size);
  assert(((size_t)arr & 3) == 0);
  arr->size = 0;
  arr->refcount = 1;

  return arr;
}

static qz_array_t* resize_array(qz_array_t* arr, size_t elem_size)
{
  /* if someone else has a reference to this array, you're gonna have a bad time */
  assert(arr->refcount == 1);

  /* do nothing if resize not needed */
  if(!is_zero_or_power_of_two(arr->size))
    return arr;

  if(arr->size < 16)
    return arr;

  /* double capacity */
  size_t capacity = arr->size * 2;

  /* make copy of array */
  qz_array_t* new_arr = (qz_array_t*)malloc(sizeof(qz_array_t) + capacity*elem_size);
  assert(((size_t)new_arr & 3) == 0);

  new_arr->size = arr->size;
  new_arr->refcount = arr->refcount;

  memcpy(QZ_ARRAY_DATA(new_arr, char), QZ_ARRAY_DATA(arr, char), arr->size*elem_size);

  /* cleanup */
  free(arr);

  return new_arr;
}

static void concat_string(char c)
{
  //printf("concat_string(%c)\n", c);

  qz_obj_t* obj = QZ_ARRAY_DATA(g_stack, qz_obj_t) + (g_stack->size - 1);
  qz_array_t* str = qz_to_string(*obj);

  /* resize if necessary */
  str = resize_array(str, sizeof(char));
  *obj = qz_from_string(str);

  /* append character */
  QZ_ARRAY_DATA(str, char)[str->size++] = c;
}

static void concat_bytevector(int b)
{
  //printf("concat_bytevector(%d)\n", b);

  qz_obj_t* obj = QZ_ARRAY_DATA(g_stack, qz_obj_t) + (g_stack->size - 1);
  qz_array_t* bvec = qz_to_bytevector(*obj);

  /* resize if necessary */
  bvec = resize_array(bvec, sizeof(uint8_t));
  *obj = qz_from_bytevector(bvec);

  /* append byte */
  QZ_ARRAY_DATA(bvec, uint8_t)[bvec->size++] = b;
}

static void append(qz_obj_t value_obj)
{
  qz_obj_t* obj = QZ_ARRAY_DATA(g_stack, qz_obj_t) + (g_stack->size - 1);

  //printf("append(%lu)\n", value_obj.value);
  //printf("  stack obj: "); qz_write(*obj, -1, stdout); fputc('\n', stdout);
  //printf("  value obj: "); qz_write(value_obj, -1, stdout); fputc('\n', stdout);

  if(qz_is_pair(*obj))
  {
    qz_pair_t* pair = qz_to_pair(*obj);

    /* walk list looking for a nil slot */
    for(;;) {
      if(qz_is_nil(pair->first)) {
        /* first unused */
        pair->first = value_obj;
        break;
      }

      if(qz_is_nil(pair->rest)) {
        /* rest unused */
        if(g_dotted_datum) {
          g_dotted_datum = 0;
          /* append directly */
          pair->rest = value_obj;
        }
        else {
          /* wrap in another cell and append */
          qz_cell_t* rest_cell = (qz_cell_t*)malloc(sizeof(qz_cell_t));
          rest_cell->type = QZ_CT_PAIR;
          rest_cell->refcount = 1;
          rest_cell->value.pair.first = value_obj;
          rest_cell->value.pair.rest = QZ_NIL;
          pair->rest = qz_from_cell(rest_cell);
        }
        break;
      }

      pair = qz_to_pair(pair->rest);
    }
  }
  else if(qz_is_vector(*obj))
  {
    qz_array_t* vec = qz_to_vector(*obj);

    /* resize if necessary */
    vec = resize_array(vec, sizeof(qz_obj_t));
    *obj = qz_from_vector(vec);

    /* append object */
    QZ_ARRAY_DATA(vec, qz_obj_t)[vec->size++] = value_obj;
  }
  else
  {
    assert(0); /* attempt to append cell to object of wrong type */
  }

  //printf("  stack obj (after): "); qz_write(*obj, -1, stdout); fputc('\n', stdout);
}

/* push an object onto the stack */
static void push(qz_obj_t obj)
{
  //printf("push()\n");

  g_stack = resize_array(g_stack, sizeof(qz_obj_t));
  QZ_ARRAY_DATA(g_stack, qz_obj_t)[g_stack->size++] = obj;
}

/* pop an object from the stack, appending it to the container at the new top of the stack */
static void pop()
{
  //printf("pop()\n");

  assert(g_stack->size > 1); /* never pop the root element */
  qz_obj_t obj = QZ_ARRAY_DATA(g_stack, qz_obj_t)[--g_stack->size];

  append(obj);
}

/* pop a string from the stack, appending the matching identifier to the container at the top of the stack */
static void pop_iden()
{
  //printf("pop_iden()\n");

  assert(g_stack->size > 1); /* never poop the root element */
  qz_obj_t obj = QZ_ARRAY_DATA(g_stack, qz_obj_t)[--g_stack->size];

  append(qz_make_iden(g_st, obj));
}

/* push a pair onto the stack */
static void push_pair()
{
  //printf("push_pair()\n");

  qz_cell_t* cell = malloc(sizeof(qz_cell_t));
  cell->type = QZ_CT_PAIR;
  cell->refcount = 1;
  cell->value.pair.first = QZ_NIL;
  cell->value.pair.rest = QZ_NIL;

  push(qz_from_cell(cell));
}

/* push a vector onto the stack */
static void push_vector()
{
  //printf("push_vector()\n");

  push(qz_from_vector(alloc_array(sizeof(qz_obj_t))));
}

/* push a bytevector onto the stack */
static void push_bytevector()
{
  //printf("push_bytevector()\n");

  push(qz_from_bytevector(alloc_array(sizeof(uint8_t))));
}

/* push a string onto the stack */
static void push_string()
{
  //printf("push_string()\n");

  push(qz_from_string(alloc_array(sizeof(char))));
}

/* append a char value to the container at the top of the stack */
static void append_char(char c)
{
  //printf("append_char(%c (%d))\n", c, c);

  append(qz_from_char(c));
}

/* append a number value to the container at the top of the stack */
static void append_number(intptr_t i)
{
  //printf("append_number(%ld)\n", i);

  append(qz_from_fixnum(i));
}

/* append a boolean value to the container at the top of the stack */
static void append_bool(int b)
{
  //printf("append_boolean(%d)\n", b);

  append(qz_from_bool(b));
}

/* append an identifier constructed from a C-style string to container at the top of the stack */
static void push_identifier_c(const char* s)
{
  //printf("push_identifier_c(%s)\n", s);

  push_string();
  while(*s)
    concat_string(*s++);
  pop_iden();
}

//#define YY_DEBUG
#define YYSTYPE intptr_t
#include "parser.c"

/* scheme's read procedure */
qz_obj_t qz_read(qz_state_t* st, FILE* fp)
{
  g_st = st;
  g_fp = fp;

  /* setup stack with root cell */
  qz_cell_t root;
  root.type = QZ_CT_PAIR;
  root.value.pair.first = QZ_NIL;
  root.value.pair.rest = QZ_NIL;

  g_stack = alloc_array(sizeof(qz_obj_t));
  push(qz_from_cell(&root));

  /* parse file */
  yyparse();

  /* cleanup */
  free(g_stack);
  g_stack = NULL;
  g_st = NULL;
  g_fp = NULL;

  return root.value.pair.first;
}

