#define NDEBUG

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "common.h"
#include "m-dict.h"


static inline bool oor_equal_p(unsigned long k, unsigned char n)
{
  return k == (unsigned long)n;
}

static inline void oor_set(unsigned long *k, unsigned char n)
{
  *k = (unsigned long)n;
}

DICT_OA_DEF2(dict,
	     unsigned long, M_OPEXTEND(M_DEFAULT_OPLIST, OOR_EQUAL(oor_equal_p), OOR_SET(oor_set M_IPTR)),
	     unsigned long, M_DEFAULT_OPLIST)

dict_t dict;
unsigned long *lockup;
size_t lockup_offset = 0;

void
init(unsigned long  n)
{
  dict_init(dict);
  for (size_t i = 0; i < n; i++) {
    dict_set_at(dict, rand_get(), rand_get() );
  }
  rand_init();
  lockup = malloc (n * sizeof(unsigned long));
  if (lockup == NULL) abort();
  for(size_t i = 0; i <n ;i++)
    lockup[i] = rand_get();
}

void
no_bulk(unsigned long n)
{
  unsigned long s = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned long *p = dict_get(dict, lockup[lockup_offset+i]);
    if (p)
      s += *p;
  }
  g_result = s;
}

inline void clflush(volatile void *p)
{
  asm volatile ("clflush (%0)" :: "r"(p));
}

#ifndef MAX_PREFETCH
#define MAX_PREFETCH 128U
#endif

void
bulk(unsigned long n)
{
  unsigned long sum = 0;
  size_t hash[MAX_PREFETCH];
  dict_pair_ct *data = dict->data;
  const size_t mask = dict->mask;	       
  for(size_t i = 0 ; i < MAX_PREFETCH; i++) {
    hash[i] = M_HASH_INT64(lockup[lockup_offset+i]) & mask;
    __builtin_prefetch(&data[hash[i]], 0, 0);
  }
  
  for (size_t i = 0; i < n; i++) {
    unsigned long key = lockup[lockup_offset+i];
    size_t p = hash[i % MAX_PREFETCH];
    if (i < n-MAX_PREFETCH) {
      hash[i % MAX_PREFETCH] = M_HASH_INT64(lockup[lockup_offset+i+MAX_PREFETCH]) & mask;
      __builtin_prefetch(&data[hash[i % MAX_PREFETCH]], 0, 0);
    }
    unsigned long ref = data[p].key;
    if (M_LIKELY (ref == key))
      sum += data[p].value;
    else if (M_LIKELY (oor_equal_p (ref, M_D1CT_OA_EMPTY)) )
      sum += 0;
    else {
      size_t s = 1;
      do {
	p = (p + M_D1CT_OA_PROBING(s)) & mask;
	if (data[p].key == key)
	  { sum += data[p].value;	break;}
      } while (!oor_equal_p(data[p].key, M_D1CT_OA_EMPTY) );
    }
    // FIXME: Needed? To measure.
    clflush(& data[p].key);
  }
  g_result = sum;
}

void bulk_get(unsigned n,
              unsigned long val[n],
              const unsigned long key[n],
              unsigned long def)
{
  size_t hash[MAX_PREFETCH];
  dict_pair_ct *data = dict->data;
  const size_t mask = dict->mask;
  M_ASSUME(n >= MAX_PREFETCH);
  for(unsigned i = 0 ; i < MAX_PREFETCH; i++) {
    hash[i] = M_HASH_INT64(key[i]) & mask;
    __builtin_prefetch(&data[hash[i]], 0, 0);
  }
  
  for (unsigned i = 0; i < n; i++) {
    unsigned long k = key[i];
    size_t p = hash[i % MAX_PREFETCH];
    if (i < n-MAX_PREFETCH) {
      hash[i % MAX_PREFETCH] = M_HASH_INT64(key[i+MAX_PREFETCH]) & mask;
      __builtin_prefetch(&data[hash[i % MAX_PREFETCH]], 0, 0);
    }
    size_t s = 1;
    while (true) {
      if (M_LIKELY (data[p].key == k)) {
        val[i] = data[p].value; // INIT_SET or SET?
        break;
      }
      if (M_LIKELY (oor_equal_p (data[p].key, M_D1CT_OA_EMPTY)) ) {
        val[i] = def;
        break;
      }
      p = (p + M_D1CT_OA_PROBING(s)) & mask;
    }
    //    clflush(& data[p].key);
  }
}

#define BUF_N 1024

void sum(unsigned long n)
{
  unsigned long sum = 0;
  unsigned long val[BUF_N];
  
  for (size_t i = 0; i < n; i += BUF_N) {
    unsigned num = M_MIN(BUF_N, n-i);
    bulk_get(num, val, lockup+lockup_offset+i, 0);
    for(unsigned j = 0; j < num; j++)
      sum += val[j];
  }
  g_result = sum;
}

/*
  z += ++*safe_get(key)
  atomic_fetch_add()
  add, sub, or, and, xor, exchange(=swap)
  bulk_[add|or|and|xor|swap](unsigned n, 
  value_type value[n],
  key_type key[n],
  value_type def);
==>
   val[i] = old(p->value)
   p->value OP= old(val[i])
==>
  bulk_add(n, value, key, operand[], 1)
 */

int main(int argc, const char *argv[])
{
  int printit = argc == 2 ? atoi(argv[1]) : 0;
  
#define N 100000000
  init(N);
  printf("INIT DONE\n");
  //test_function("SEQU", N/128, no_bulk);
  //test_function("BULK", N/128, bulk);

  for(int i = 0; i < 50; i ++) {
    lockup_offset += N/64;
    if (printit == 1)
      test_function("SEQU", N/64, no_bulk);
    else if (printit == 2)
      test_function("BULK+GET", N/64, sum);
    else
      test_function("BULK", N/64, bulk);
  }
}
