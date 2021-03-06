#include "quuz.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* city.cc */
uint32_t CityHash32(const char *s, size_t len);

static uint32_t hash_obj(qz_obj_t obj)
{
  if(qz_is_cell(obj))
  {
    qz_cell_t* cell = qz_to_cell(obj);
    qz_cell_type_t type = qz_type(cell);

    if(type == QZ_CT_STRING)
      return CityHash32(QZ_CELL_DATA(cell, char), cell->value.array.size*sizeof(char));

    assert(0); /* can't hash this type */
  }

  return CityHash32((char*)&obj, sizeof(obj));
}

/* create a new hash object with the given capacity */
static qz_cell_t* make_hash(size_t capacity)
{
  qz_cell_t* cell = qz_make_cell(QZ_CT_HASH, capacity*sizeof(qz_pair_t));
  cell->value.array.size = 0;
  cell->value.array.capacity = capacity;

  qz_pair_t* data = QZ_CELL_DATA(cell, qz_pair_t);

  for(size_t i = 0; i < capacity; i++) {
    data[i].first = QZ_NONE;
    data[i].rest = QZ_NONE;
  }

  return cell;
}

/* finds the pair where the given key is or would be stored */
static qz_pair_t* get_hash(qz_cell_t* cell, qz_obj_t key)
{
  assert(qz_type(cell) == QZ_CT_HASH);

  qz_pair_t* data = QZ_CELL_DATA(cell, qz_pair_t);

  for(uint32_t i = hash_obj(key); /**/; i++)
  {
    size_t slot = i % cell->value.array.capacity;

    if(qz_is_none(data[slot].first) || qz_equal(data[slot].first, key))
      return &data[slot];
  }
}

/* reallocates the given hash, doubling its capacity */
static void realloc_hash(qz_obj_t* obj)
{
  qz_cell_t* cell = qz_to_cell(*obj);
  assert(qz_refcount(cell) == 1);

  /* make new hash */
  qz_cell_t* new_cell = make_hash(cell->value.array.capacity * 2);

  new_cell->info = cell->info; /* collector fields must be copied */
  new_cell->value.array.size = cell->value.array.size;

  /* copy pairs to new hash */
  for(size_t i = 0; i < cell->value.array.capacity; i++)
  {
    qz_pair_t* pair = QZ_CELL_DATA(cell, qz_pair_t) + i;

    if(qz_is_none(pair->first) || qz_is_none(pair->rest))
      continue;

    qz_pair_t* new_pair = get_hash(new_cell, pair->first);

    *new_pair = *pair;
  }

  /* replace old cell with new */
  free(cell);
  *obj = qz_from_cell(new_cell);
}

qz_obj_t qz_make_hash(void)
{
  return qz_from_cell(make_hash(4));
}

qz_obj_t* qz_hash_get(qz_state_t* st, qz_obj_t obj, qz_obj_t key)
{
  QZ_UNUSED(st);
  qz_pair_t* pair = get_hash(qz_to_cell(obj), key);

  if(qz_is_none(pair->first))
    return NULL;

  return &pair->rest;
}

void qz_hash_set(qz_state_t* st, qz_obj_t* obj, qz_obj_t key, qz_obj_t value)
{
  qz_cell_t* cell = qz_to_cell(*obj);
  qz_pair_t* pair = get_hash(cell, key);

  if(qz_is_none(pair->first))
    cell->value.array.size++;

  qz_unref(st, pair->first);
  pair->first = key;
  qz_unref(st, pair->rest);
  pair->rest = value;

  if(cell->value.array.size * 10 > cell->value.array.capacity * 7)
    realloc_hash(obj);
}
