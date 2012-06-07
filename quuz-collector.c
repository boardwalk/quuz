#include "quuz.h"
#include <assert.h>
#include <stdlib.h>

/*void describe(qz_cell_t*);

void logit(const char* fn, qz_cell_t* cell)
{
  fprintf(stderr, "%s(", fn);
  describe(cell);
  fputs(")\n", stderr);
}

#define LOGIT logit(__FUNCTION__, cell)*/

/* The algorithm here is an implementation of "Synchronous Cycle Collection" described in
 * http://www.research.ibm.com/people/d/dfb/papers/Bacon01Concurrent.pdf */

typedef void (*child_func)(qz_state_t* st, qz_cell_t* cell);

static void call_if_valid_cell(qz_state_t* st, qz_obj_t obj, child_func func)
{
  if(qz_is_cell(obj) && !qz_is_nil(obj))
    func(st, qz_to_cell(obj));
}

static void all_children(qz_state_t* st, qz_cell_t* cell, child_func func)
{
  if(qz_type(cell) == QZ_CT_PAIR || qz_type(cell) == QZ_CT_FUN)
  {
    call_if_valid_cell(st, cell->value.pair.first, func);
    call_if_valid_cell(st, cell->value.pair.rest, func);
  }
  else if(qz_type(cell) == QZ_CT_VECTOR)
  {
    for(size_t i = 0; i < cell->value.array.size; i++) {
      qz_obj_t* obj = QZ_CELL_DATA(cell, qz_obj_t) + i;
      call_if_valid_cell(st, *obj, func);
    }
  }
  else if(qz_type(cell) == QZ_CT_HASH)
  {
    for(size_t i = 0; i < cell->value.array.capacity; i++) {
      qz_pair_t* pair = QZ_CELL_DATA(cell, qz_pair_t) + i;
      call_if_valid_cell(st, pair->first, func);
      call_if_valid_cell(st, pair->rest, func);
    }
  }
}

static void mark_gray(qz_state_t* st, qz_cell_t* cell);
static void scan_black(qz_state_t* st, qz_cell_t* cell);
static void release(qz_state_t* st, qz_cell_t* cell);

/* mark_roots related */
static void decr_and_mark_gray(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  assert(qz_refcount(cell) != 0);
  qz_set_refcount(cell, qz_refcount(cell) - 1);
  mark_gray(st, cell);
}

static void mark_gray(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  if(qz_color(cell) != QZ_CC_GRAY)
  {
    qz_set_color(cell, QZ_CC_GRAY);
    all_children(st, cell, decr_and_mark_gray);
  }
}

static void mark_roots(qz_state_t* st)
{
  for(size_t i = 0; i < st->root_buffer_size; /**/)
  {
    qz_cell_t* cell = st->root_buffer[i];
    /*LOGIT;*/

    if(qz_color(cell) == QZ_CC_PURPLE)
    {
      mark_gray(st, cell);
      i++;
    }
    else
    {
      qz_set_buffered(cell, 0);

      // remove S from Roots
      for(size_t j = i + 1; j < st->root_buffer_size; j++)
        st->root_buffer[j - 1] = st->root_buffer[j];
      st->root_buffer_size--;

      if(qz_color(cell) == QZ_CC_BLACK && qz_refcount(cell) == 0) {
        /*fprintf(stderr, "mark_roots free(%p)\n", (void*)cell);*/
        free(cell);
      }
    }
  }
}

/* scan_roots related */
static void incr_and_scan_black(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  qz_set_refcount(cell, qz_refcount(cell) + 1);
  if(qz_color(cell) != QZ_CC_BLACK)
    scan_black(st, cell);
}

static void scan_black(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  qz_set_color(cell, QZ_CC_BLACK);
  all_children(st, cell, incr_and_scan_black);
}

static void scan(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  if(qz_color(cell) == QZ_CC_GRAY)
  {
    if(qz_refcount(cell) > 0)
    {
      scan_black(st, cell);
    }
    else
    {
      qz_set_color(cell, QZ_CC_WHITE);
      all_children(st, cell, scan);
    }
  }
}

static void scan_roots(qz_state_t* st)
{
  for(size_t i = 0; i < st->root_buffer_size; i++)
    scan(st, st->root_buffer[i]);
}

/* collect_roots related */
static void collect_white(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  if(qz_color(cell) == QZ_CC_WHITE && !qz_buffered(cell))
  {
    qz_set_color(cell, QZ_CC_BLACK);
    all_children(st, cell, collect_white);
    free(cell);
  }
}

static void collect_roots(qz_state_t* st)
{
  /* the paper say we should collect_white() here,
    * but we can't because it frees the cell and we may (will?)
    * come back to it when running the collect_white on another root,
    * so we're partially duplicating collect_white() here and adding
    * another loop to clean up unreferenced cells after */
  for(size_t i = 0; i < st->root_buffer_size; i++)
  {
    qz_cell_t* cell = st->root_buffer[i];
    /*LOGIT;*/
    qz_set_buffered(cell, 0);
    if(qz_color(cell) == QZ_CC_WHITE) {
      qz_set_color(cell, QZ_CC_BLACK);
      all_children(st, cell, collect_white);
    }
  }

  for(size_t i = 0; i < st->root_buffer_size; i++)
  {
    qz_cell_t* cell = st->root_buffer[i];
    if(qz_color(cell) == QZ_CC_BLACK && qz_refcount(cell) == 0)
      free(cell);
  }

  st->root_buffer_size = 0;
}

static void possible_root(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  if(qz_color(cell) != QZ_CC_PURPLE)
  {
    qz_set_color(cell, QZ_CC_PURPLE);

    if(!qz_buffered(cell))
    {
      qz_set_buffered(cell, 1);
      st->root_buffer[st->root_buffer_size++] = cell;

      if(st->root_buffer_size == QZ_ROOT_BUFFER_CAPACITY)
        qz_collect(st);
    }
  }
}

static void increment(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  qz_set_refcount(cell, qz_refcount(cell) + 1);
  qz_set_color(cell, QZ_CC_BLACK);
}

static void decrement(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  assert(qz_refcount(cell) != 0);
  size_t refcount = qz_refcount(cell) - 1;
  qz_set_refcount(cell, refcount);
  if(refcount == 0)
    release(st, cell);
  else
    possible_root(st, cell);
}

static void release(qz_state_t* st, qz_cell_t* cell)
{
  /*LOGIT;*/
  all_children(st, cell, decrement);
  qz_set_color(cell, QZ_CC_BLACK);
  if(!qz_buffered(cell)) {
    /*fprintf(stderr, "release free(%p)\n", (void*)cell);*/
    free(cell); /* I never liked that game */
  }
}

/* public functions */
qz_obj_t qz_ref(qz_state_t* st, qz_obj_t obj)
{
  call_if_valid_cell(st, obj, increment);
  return obj;
}

void qz_unref(qz_state_t* st, qz_obj_t obj)
{
  call_if_valid_cell(st, obj, decrement);
}

void qz_collect(qz_state_t* st)
{
  /*fprintf(stderr, "mark_roots starting...\n");*/
  mark_roots(st);
  /*fprintf(stderr, "scan roots starting...\n");*/
  scan_roots(st);
  /*fprintf(stderr, "collect_roots starting...\n");*/
  collect_roots(st);
  /*fprintf(stderr, "qz_collect done\n");*/
}

