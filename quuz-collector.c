#include "quuz.h"
#include <assert.h>
#include <stdlib.h>

/* The algorithm here is an implementation of "Synchronous Cycle Collection" described in
 * http://www.research.ibm.com/people/d/dfb/papers/Bacon01Concurrent.pdf */

static void collect_cycles(qz_state_t* st)
{
  /* FIXME */
  st->root_buffer_size = 0;
}

static void possible_root(qz_state_t* st, qz_obj_t obj)
{
  qz_cell_t* cell = qz_to_cell(obj);

  qz_set_color(cell, QZ_CC_PURPLE);

  if(!qz_buffered(cell))
  {
    qz_set_buffered(cell, 1);
    st->root_buffer[st->root_buffer_size++] = obj;

    if(st->root_buffer_size == QZ_ROOT_BUFFER_CAPACITY)
      collect_cycles(st);
  }
}

static void release(qz_state_t* st, qz_obj_t obj)
{
  qz_cell_t* cell = qz_to_cell(obj);

  if(qz_type(cell) == QZ_CT_PAIR || qz_type(cell) == QZ_CT_FUN) {
    qz_unref(st, cell->value.pair.first);
    qz_unref(st, cell->value.pair.rest);
  }
  else if(qz_type(cell) == QZ_CT_STRING) {
    /* nothing to do */
  }
  else if(qz_type(cell) == QZ_CT_VECTOR) {
    qz_obj_t* data = QZ_CELL_DATA(cell, qz_obj_t);
    for(size_t i = 0; i < cell->value.array.size; i++)
      qz_unref(st, data[i]);
  }
  else if(qz_type(cell) == QZ_CT_BYTEVECTOR) {
    /* nothing to do */
  }
  else if(qz_type(cell) == QZ_CT_HASH) {
    qz_pair_t* data = QZ_CELL_DATA(cell, qz_pair_t);
    for(size_t i = 0; i < cell->value.array.capacity; i++) {
      qz_unref(st, data[i].first);
      qz_unref(st, data[i].rest);
    }
  }
  else if(qz_type(cell) == QZ_CT_REAL) {
    /* nothing to do */
  }
  else {
    assert(0); /* unknown cell type */
  }

  free(cell); /* I never liked that game */
}

qz_obj_t qz_ref(qz_state_t* st, qz_obj_t obj)
{
  if(qz_is_cell(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell) {
      qz_set_refcount(cell, qz_refcount(cell) + 1);
      qz_set_color(cell, QZ_CC_BLACK);
    }
  }
  return obj;
}

void qz_unref(qz_state_t* st, qz_obj_t obj)
{
  if(qz_is_cell(obj)) {
    qz_cell_t* cell = qz_to_cell(obj);
    if(cell) {
      size_t refcount = qz_refcount(cell) - 1;
      qz_set_refcount(cell, refcount);
      if(refcount)
        possible_root(st, obj);
      else
        release(st, obj);
    }
  }
}

