#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define YY_INPUT(buf, result, max_size)            \
  {                                                \
    int yyc = fgetc(g_fp);                         \
    result = (EOF == yyc) ? 0 : (*(buf) = yyc, 1); \
    yyprintf((stderr, "<%c>", yyc));               \
  }

static qz_string_t null_string = {0};
static qz_vector_t null_vector = {0};
static qz_bytevector_t null_bytevector = {0};

static FILE* g_fp = NULL;
static qz_vector_t* g_stack = &null_vector;
static int g_dotted_datum = 0;

static int is_zero_or_power_of_two(size_t v)
{
  return !(v & (v - 1));
}

static void* resize_array(void* ptr, size_t elem_size)
{
  typedef struct {
    size_t size;
    char data[];
  } array_t;

  array_t* arr = (array_t*)ptr;

  /* do nothing if resize not needed */
  if(!is_zero_or_power_of_two(arr->size))
    return arr;

  if(arr->size > 0 && arr->size < 16)
    return arr;

  /* double capacity */
  size_t capacity = (arr->size >= 16) ? (arr->size * 2) : 16;

  /* make copy of array */
  array_t* new_arr = (array_t*)malloc(sizeof(array_t) + capacity*elem_size);
  assert(((size_t)new_arr & 7) == 0);
  new_arr->size = arr->size;
  memcpy(new_arr->data, arr->data, arr->size*elem_size);

  /* cleanup */
  if(arr->size) free(arr);

  //printf("resize_array: arr=%p new_arr=%p size=%lu capacity=%lu\n", arr, new_arr, new_arr->size, capacity);

  return new_arr;
}

static void concat_string(char c)
{
  //printf("concat_string(%c)\n", c);

  qz_obj_t* obj = &g_stack->data[g_stack->size - 1];
  qz_string_t* str = qz_to_string(*obj);

  /* resize if necessary */
  str = (qz_string_t*)resize_array(str, sizeof(char));
  *obj = qz_from_string(str);

  /* append character */
  str->data[str->size++] = c;
}

static void concat_identifier(char c)
{
  //printf("concat_identifier(%c)\n", c);
  
  qz_obj_t* obj = &g_stack->data[g_stack->size - 1];
  qz_string_t* str = qz_to_identifier(*obj);

  /* resize if necessary */
  str = (qz_string_t*)resize_array(str, sizeof(char));
  *obj = qz_from_identifier(str);

  /* append character */
  str->data[str->size++] = c;
}

static void concat_bytevector(int b)
{
  //printf("concat_bytevector(%d)\n", b);

  qz_obj_t* obj = &g_stack->data[g_stack->size - 1];
  qz_bytevector_t* bv = qz_to_bytevector(*obj);

  /* resize if necessary */
  bv = (qz_bytevector_t*)resize_array(bv, sizeof(uint8_t));
  *obj = qz_from_bytevector(bv);

  /* append byte */
  bv->data[bv->size++] = b;
}

static void append(qz_obj_t value_obj)
{
  qz_obj_t* obj = &g_stack->data[g_stack->size - 1];

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
          rest_cell->pair.first = value_obj;
          rest_cell->pair.rest = QZ_NIL;
          pair->rest = qz_from_cell(rest_cell);
        }
        break;
      }

      pair = qz_to_pair(pair->rest);
    }
  }
  else if(qz_is_vector(*obj))
  {
    qz_vector_t* vec = qz_to_vector(*obj);

    /* resize if necessary */
    vec = (qz_vector_t*)resize_array(vec, sizeof(qz_obj_t));
    *obj = qz_from_vector(vec);

    /* append object */
    vec->data[vec->size++] = value_obj;
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

  g_stack = (qz_vector_t*)resize_array(g_stack, sizeof(qz_obj_t));
  g_stack->data[g_stack->size++] = obj;
}

/* pop an object from the stack, appending it to the container at the new top of the stack */
static void pop()
{
  //printf("pop()\n");

  assert(g_stack->size > 1); /* never pop the root element */
  qz_obj_t obj = g_stack->data[--g_stack->size];

  append(obj);
}

/* push a pair onto the stack */
static void push_pair()
{
  //printf("push_pair()\n");

  qz_cell_t* cell = malloc(sizeof(qz_cell_t));
  cell->type = QZ_CT_PAIR;
  cell->pair.first = QZ_NIL;
  cell->pair.rest = QZ_NIL;

  push(qz_from_cell(cell));
}

/* push a vector onto the stack */
static void push_vector()
{
  //printf("push_vector()\n");

  push(qz_from_vector(&null_vector));
}

/* push a bytevector onto the stack */
static void push_bytevector()
{
  //printf("push_bytevector()\n");

  push(qz_from_bytevector(&null_bytevector));
}

/* push a string onto the stack */
static void push_string()
{
  //printf("push_string()\n");

  push(qz_from_string(&null_string));
}

/* push an identifier onto the stack */
static void push_identifier()
{
  //printf("push_identifier()\n");

  push(qz_from_identifier(&null_string));
}

/* append a char value to the container at the top of the stack */
static void append_char(char c)
{
  //printf("append_char(%c (%d))\n", c, c);

  append(qz_from_char(c));
}

/* append a number value to the container at the top of the stack */
static void append_number(ssize_t i)
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

  push_identifier();
  while(*s)
    concat_identifier(*s++);
  pop();
}

//#define YY_DEBUG
#include "parser.c"

/* scheme's read procedure */
qz_obj_t qz_read(FILE* fp)
{
  /* setup stack with root cell */
  qz_cell_t root;
  root.type = QZ_CT_PAIR;
  root.pair.first = QZ_NIL;
  root.pair.rest = QZ_NIL;

  g_stack = &null_vector;
  push(qz_from_cell(&root));

  /* setup input file */
  g_fp = fp;

  /* parse file */
  while(yyparse()) /**/;

  /* cleanup */
  free(g_stack); /* will always contain the root element, hence not be null_vector */
  g_stack = NULL;
  g_fp = NULL;

  return root.pair.first;
}

