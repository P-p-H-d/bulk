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

void
bulk(unsigned long n)
{
  unsigned long sum = 0;
#ifndef MAX_PREFETCH
#define MAX_PREFETCH 128U
#endif
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
    clflush(& data[p].key);
  }
  g_result = sum;
}

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
    if (printit != 0)
      test_function("SEQU", N/64, no_bulk);
    else
      test_function("BULK", N/64, bulk);
  }
}
