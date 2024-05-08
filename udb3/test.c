#include "common.c"
#include "m-dict.h"

static inline int oor_equal_p(uint32_t k, unsigned char n)
{
  return (k == (uint32_t)(-n-1));
}

static inline void oor_set(uint32_t *k, unsigned char n)
{
  *k = (uint32_t)(-n-1);
}

DICT_OA_DEF2(intmap, uint32_t, M_OPEXTEND(M_BASIC_OPLIST, HASH(udb_hash_fn), OOR_EQUAL(oor_equal_p), OOR_SET(API_2(oor_set))), uint32_t, M_BASIC_OPLIST)

#ifndef BULK_MODE
void test_int(uint32_t N, uint32_t n0, int32_t is_del, uint32_t x0, uint32_t n_cp, udb_checkpoint_t *cp)
{
  uint32_t step = (N - n0) / (n_cp - 1);
  uint32_t i, n, j;
  uint64_t z = 0, x = x0;
  intmap_t h;
  intmap_init(h);
  for (j = 0, i = 0, n = n0; j < n_cp; ++j, n += step) {
    for (; i < n; ++i) {
      uint64_t y = udb_splitmix64(&x);
      uint32_t key = udb_get_key(n, y);
      uint32_t *p = intmap_get(h, key);
      if (is_del) {
        if (p == 0) {
          intmap_set_at(h, key, i);
          ++z;
        } else intmap_erase(h, key);
      } else {
        if (p == 0) {
          intmap_set_at(h, key, 1);
          ++z;
        } else z += ++*p;
      }
    }
    udb_measure(n, intmap_size(h), z, &cp[j]);
  }
  intmap_clear(h);
}
#else

#ifndef MAX_PREFETCH
#define MAX_PREFETCH 64U
#endif

void clflush(volatile void *p)
{
  asm volatile ("clflush (%0)" :: "r"(p));
}

void bulk_add(intmap_t dict, unsigned n, uint32_t val[n], uint32_t key[n], uint32_t def)
{
  size_t hash[MAX_PREFETCH];
  intmap_pair_ct *data = dict->data;
  size_t mask = dict->mask;
  M_ASSUME(n >= MAX_PREFETCH);
  for(unsigned i = 0 ; i < MAX_PREFETCH; i++) {
    hash[i] = udb_hash_fn(key[i]);
    __builtin_prefetch(&data[hash[i] & mask], 0, 0);
  }
  
  for (unsigned i = 0; i < n; i++) {
    unsigned long k = key[i];
    size_t p = hash[i % MAX_PREFETCH] & mask;
    if (i < n-MAX_PREFETCH) {
      hash[i % MAX_PREFETCH] = udb_hash_fn(key[i+MAX_PREFETCH]);
      __builtin_prefetch(&data[hash[i % MAX_PREFETCH] & mask], 0, 0);
    }
    unsigned long ref = data[p].key;
    if (M_LIKELY (ref == k)) {
      data[p].value += val[i];
      val[i] = data[p].value;
      goto next;     
    }
    // Search loop for insertion
    size_t s = 1;
    size_t delPos = SIZE_MAX;
    while (M_UNLIKELY (!oor_equal_p (data[p].key, M_D1CT_OA_EMPTY)) ) {
      if (data[p].key == k) {
        // Found, no need for insertion
        data[p].value += val[i];
        val[i] = data[p].value;
        goto next;
      }
      if (oor_equal_p (data[p].key, M_D1CT_OA_DELETED)
          && (delPos == SIZE_MAX))                          
        delPos = p;                                                     
      p = (p + M_D1CT_OA_PROBING(s)) & mask;
    }
    // Add new items
    if (delPos != SIZE_MAX) {                                           
      p = delPos;                                                       
      dict->count_delete --;                                            
    }                                                                   
    data[p].key = key[i];
    val[i] += def;
    data[p].value = val[i];
    dict->count++;                                                      
    dict->count_delete ++;                                                    
                                                                              
    if (M_UNLIKELY (dict->count_delete >= dict->upper_limit)) {               
      size_t newSize = dict->mask+1;                                          
      if (dict->count > (dict->mask / 2)) {                                   
        newSize += newSize;                                                   
        if (M_UNLIKELY_NOMEM (newSize <= dict->mask+1)) {                     
          M_MEMORY_FULL((size_t)-1);                                          
        }                                                               
      }                                                                 
      m_d1ct_intmap_resize_up(dict, newSize, true);
      mask = dict->mask;
      data = dict->data;
      continue;
    } 
  next:
    // FIXME: Needed? To measure.
    clflush(& data[p].key);
  }
}

volatile unsigned g_counter;
static inline void update1(intmap_t h, uint32_t key, uint32_t *oldval, uint32_t *newval)
{
  (void) key;
  *newval += *oldval;
  *oldval = *newval;
}

void bulk_update(intmap_t dict, unsigned n, uint32_t val[n], uint32_t key[n], uint32_t def,
                 void (*update)(intmap_t dict, uint32_t key, uint32_t *newval, uint32_t *preval))
{
  size_t hash[MAX_PREFETCH];
  M_ASSUME(n >= MAX_PREFETCH);
  for(unsigned i = 0 ; i < MAX_PREFETCH; i++) {
    hash[i] = udb_hash_fn(key[i]);
    __builtin_prefetch(&dict->data[hash[i] & dict->mask], 0, 0);
  }
  
  for (unsigned i = 0; i < n; i++) {
    unsigned long k = key[i];
    size_t p = hash[i % MAX_PREFETCH] & dict->mask;
    if (i < n-MAX_PREFETCH) {
      hash[i % MAX_PREFETCH] = udb_hash_fn(key[i+MAX_PREFETCH]);
      __builtin_prefetch(&dict->data[hash[i % MAX_PREFETCH] & dict->mask], 0, 0);
    }
    unsigned long ref = dict->data[p].key;
    if (M_LIKELY (ref == k)) {
      update(dict, k, &dict->data[p].value, &val[i]);
      goto next;     
    }
    // Search loop for insertion
    size_t s = 1;
    size_t delPos = SIZE_MAX;
    while (M_UNLIKELY (!oor_equal_p (dict->data[p].key, M_D1CT_OA_EMPTY)) ) {
      if (dict->data[p].key == k) {
        // Found, no need for insertion
        update(dict, k, &dict->data[p].value, &val[i]);
        goto next;
      }
      if (oor_equal_p (dict->data[p].key, M_D1CT_OA_DELETED)
          && (delPos == SIZE_MAX))                          
        delPos = p;                                                     
      p = (p + M_D1CT_OA_PROBING(s)) & dict->mask;
    }
    // Add new items
    if (delPos != SIZE_MAX) {                                           
      p = delPos;                                                       
      dict->count_delete --;                                            
    }
    dict->data[p].key = k;
    dict->data[p].value = def;
    update(dict, k, &dict->data[p].value, &val[i]);
    dict->count++;                                                      
    dict->count_delete ++;                                                    
                                                                              
    if (M_UNLIKELY (dict->count_delete >= dict->upper_limit)) {               
      size_t newSize = dict->mask+1;                                          
      if (dict->count > (dict->mask / 2)) {                                   
        newSize += newSize;                                                   
        if (M_UNLIKELY_NOMEM (newSize <= dict->mask+1)) {                     
          M_MEMORY_FULL((size_t)-1);                                          
        }                                                               
      }                                                                 
      m_d1ct_intmap_resize_up(dict, newSize, true);
      continue;
    } 
  next:
    // FIXME: Needed? To measure.
    clflush(& dict->data[p].key);
  }
}

static inline void update2(intmap_t h, uint32_t key, uint32_t *oldval, uint32_t *newval)
{
  if (*oldval == 0xFFFFFFFF) {
    *oldval = *newval;
    *newval = 1;
  } else {
    intmap_erase(h, key);
    *newval = 0;
  }
}

void test_int(uint32_t N, uint32_t n0, int32_t is_del, uint32_t x0, uint32_t n_cp, udb_checkpoint_t *cp)
{
  uint32_t step = (N - n0) / (n_cp - 1);
  uint32_t i, n, j;
  uint64_t z = 0, x = x0;
  intmap_t h;
#define BUF_N 1024
  uint32_t values[BUF_N];
  uint32_t keys[BUF_N];
  intmap_init(h);
  for (j = 0, i = 0, n = n0; j < n_cp; ++j, n += step) {
    if (is_del) {
#if BULK_MODE == 1
      abort();
#endif
      while (i < n) {
        unsigned num = M_MIN(BUF_N, n-i);
        for(unsigned k = 0; k < num ; k++) {
          values[k] = i + k;
          uint64_t y = udb_splitmix64(&x);
          uint32_t key = udb_get_key(n, y);
          keys[k] = key;
        }
        bulk_update(h, num, values, keys, -1, update2);
        for(unsigned k = 0; k < num ; k++) {
          z += values[k];
        }
        i += num;
      }
    } else {
      while (i < n) {
        unsigned num = M_MIN(BUF_N, n-i);
        for(unsigned k = 0; k < num ; k++) {
          values[k] = 1;
          uint64_t y = udb_splitmix64(&x);
          uint32_t key = udb_get_key(n, y);
          keys[k] = key;
        }
#if BULK_MODE == 1
        bulk_add(h, num, values, keys, 0);
#elif BULK_MODE == 2
        bulk_update(h, num, values, keys, 0, update1);
#endif
        for(unsigned k = 0; k < num ; k++) {
          z += values[k];
        }
        i += num;
      }
    }
    udb_measure(n, intmap_size(h), z, &cp[j]);
  }
  intmap_clear(h);
}
#endif
