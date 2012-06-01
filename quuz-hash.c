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
  if(qz_is_fixnum(obj) || qz_is_bool(obj) || qz_is_char(obj))
  {
    return hash_mem(&obj, sizeof(obj), seed);
  }
  else if(qz_is_string(obj))
  {
    qz_array_t* str = qz_to_string(obj);
    return hash_mem(QZ_ARRAY_DATA(str, char), str->size*sizeof(char), seed);
  }
  else if(qz_is_identifier(obj))
  {
    qz_array_t* iden = qz_to_string(obj);
    return hash_mem(QZ_ARRAY_DATA(iden, char), iden->size*sizeof(char), seed);
  }
  else if(qz_is_bytevector(obj))
  {
    qz_array_t* bvec = qz_to_string(obj);
    return hash_mem(QZ_ARRAY_DATA(bvec, uint8_t), bvec->size*sizeof(uint8_t), seed);
  }
  else if(qz_is_vector(obj))
  {
    qz_array_t* vec = qz_to_vector(obj);
    qz_obj_t* data = QZ_ARRAY_DATA(vec, qz_obj_t);
    for(size_t i = 0; i < vec->size; i++)
      seed = inner_hash(data[i], seed);
    return seed;
  }
  else if(qz_is_real(obj))
  {
    double real = qz_to_real(obj);
    return hash_mem(&real, sizeof(real), seed);
  }
  else if(qz_is_pair(obj))
  {
    qz_pair_t* pair = qz_to_pair(obj);
    seed = inner_hash(pair->first, seed);
    return inner_hash(pair->rest, seed);
  }
  else if(qz_is_nil(obj))
  {
    return seed;
  }

  assert(0); /* can't hash this type */
}

static uint32_t hash_obj(qz_obj_t o)
{
  return inner_hash(o, 0xDEADBEEF);
}

/* create a new, empty hash object with the given capacity */
static qz_cell_t* hash_create(int capacity)
{
  qz_cell_t* cell = (qz_cell_t*)malloc(sizeof(qz_cell_t) + capacity*sizeof(qz_pair_t));
  cell->type = QZ_CT_HASH;
  cell->refcount = 1;
  cell->value.hash.size = 0;
  cell->value.hash.capacity = capacity;

  qz_pair_t* pairs = QZ_HASH_DATA(&cell->value.hash);

  for(size_t i = 0; i < capacity; i++) {
    pairs[i].first = QZ_NIL;
    pairs[i].rest = QZ_NIL;
  }

  return cell;
}

/* finds the pair where the given key is or would be stored */
static qz_pair_t* hash_get(qz_hash_t* hash, qz_obj_t key)
{
  for(uint32_t i = hash_obj(key); /**/; i++)
  {
    qz_pair_t* pair = QZ_HASH_DATA(hash) + (i % hash->capacity);

    if(qz_is_nil(pair->first) || qz_equal(pair->first, key))
      return pair;
  }
}

/* reallocates the given hash, doubling its capacity */
static void hash_realloc(qz_obj_t* obj)
{
  qz_cell_t* cell = qz_to_cell(*obj);
  qz_hash_t* hash = &cell->value.hash;

  qz_cell_t* new_cell = hash_create(hash->capacity * 2);
  qz_hash_t* new_hash = &new_cell->value.hash;

  for(size_t i = 0; i < hash->capacity; i++)
  {
    qz_pair_t* pair = QZ_HASH_DATA(hash) + i;

    if(qz_is_nil(pair->first) || qz_is_nil(pair->rest))
      continue;

    qz_pair_t* new_pair = hash_get(new_hash, pair->first);

    *new_pair = *pair;
  }

  // replace old cell with new
  new_cell->refcount = cell->refcount;
  free(cell);
  *obj = qz_from_cell(new_cell);
}

/* create a new, empty hash object */
qz_obj_t qz_hash_create()
{
  return qz_from_cell(hash_create(16));
}

/* retrieve a value from a hash object given a key
 * returns QZ_NIL if not found */
qz_obj_t qz_hash_get(qz_obj_t obj, qz_obj_t key)
{
  return hash_get(qz_to_hash(obj), key)->rest;
}

/* set a value into a hash object given a key
 * obj may be rewritten if reallocated */
void qz_hash_set(qz_obj_t* obj, qz_obj_t key, qz_obj_t value)
{
  qz_hash_t* hash = qz_to_hash(*obj);
  qz_pair_t* pair = hash_get(hash, key);

  if(qz_is_nil(pair->first))
    hash->size++;

  qz_assign(&pair->first, key);
  qz_assign(&pair->rest, value);

  if(hash->size * 10 > hash->capacity * 7)
    hash_realloc(obj);
}

