#include "quuz.h"
#include "MurmurHash3.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static uint32_t hash_mem(const void* key, int len, uint32_t seed)
{
  MurmurHash3_x86_32(key, len, seed, &seed);
  return seed;
}

static uint32_t inner_hash(qz_obj_t obj, uint32_t seed)
{
  if(qz_is_cell(obj))
  {
    qz_cell_t* cell = qz_to_cell(obj);
    qz_cell_type_t type = qz_type(cell);

    if(type == QZ_CT_STRING)
      return hash_mem(QZ_CELL_DATA(cell, char), cell->value.array.size*sizeof(char), seed);

    assert(0); /* can't hash this type */
  }

  return hash_mem(&obj, sizeof(obj), seed);
}

static uint32_t hash_obj(qz_obj_t obj)
{
  return inner_hash(obj, 0xDEADBEEF);
}

/* create a new hash object with the given capacity */
static qz_cell_t* make_hash(int capacity)
{
  qz_cell_t* cell = qz_make_cell(QZ_CT_HASH, capacity*sizeof(qz_pair_t));
  cell->value.array.size = 0;
  cell->value.array.capacity = capacity;

  qz_pair_t* data = QZ_CELL_DATA(cell, qz_pair_t);

  for(size_t i = 0; i < capacity; i++) {
    data[i].first = QZ_NIL;
    data[i].rest = QZ_NIL;
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

    if(qz_is_nil(data[slot].first) || qz_equal(data[slot].first, key))
      return &data[slot];
  }
}

/* reallocates the given hash, doubling its capacity */
static void realloc_hash(qz_obj_t* obj)
{
  qz_cell_t* cell = qz_to_cell(*obj);
  assert(qz_refcount(cell) == 1);

  qz_cell_t* new_cell = make_hash(cell->value.array.capacity * 2);

  for(size_t i = 0; i < cell->value.array.capacity; i++)
  {
    qz_pair_t* pair = QZ_CELL_DATA(cell, qz_pair_t) + i;

    if(qz_is_nil(pair->first) || qz_is_nil(pair->rest))
      continue;

    qz_pair_t* new_pair = get_hash(new_cell, pair->first);

    *new_pair = *pair;
  }

  // replace old cell with new
  new_cell->info = cell->info;
  free(cell);
  *obj = qz_from_cell(new_cell);
}

/* create a new hash object */
qz_obj_t qz_make_hash()
{
  return qz_from_cell(make_hash(8));
}

/* retrieve a value from a hash object given a key
 * returns QZ_NIL if not found */
qz_obj_t qz_get_hash(qz_state_t* st, qz_obj_t obj, qz_obj_t key)
{
  return get_hash(qz_to_cell(obj), key)->rest;
}

/* set a value into a hash object given a key
 * steals a reference from both key and value
 * obj may be rewritten if reallocated */
void qz_set_hash(qz_state_t* st, qz_obj_t* obj, qz_obj_t key, qz_obj_t value)
{
  qz_cell_t* cell = qz_to_cell(*obj);
  qz_pair_t* pair = get_hash(cell, key);

  if(qz_is_nil(pair->first))
    cell->value.array.size++;

  qz_unref(st, pair->first);
  pair->first = key;
  qz_unref(st, pair->rest);
  pair->rest = value;

  if(cell->value.array.size * 10 > cell->value.array.capacity * 7)
    realloc_hash(obj);
}

