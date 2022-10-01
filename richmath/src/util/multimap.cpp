#include <util/multimap.h>


using namespace richmath;

static_assert(sizeof(MultiMapEntry<int32_t, int32_t>) == 12, "");
static_assert(sizeof(MultiMapEntry<int64_t, int32_t>) == 16, "");
static_assert(sizeof(MultiMapEntry<int32_t, int64_t>) == 16, "");

#ifndef NDEBUG

static void debug_test_multimap_single();
static void debug_test_multimap_multi();
static void debug_test_multimap_delete();

void richmath::debug_test_multimap() {
  debug_test_multimap_single();
  debug_test_multimap_multi();
  debug_test_multimap_delete();
}

static void debug_test_multimap_single() {
  MultiMap<int, int> mmap;
  
  int count = 0;
  for(auto v : mmap[1])
    ++count;
  
  HASHTABLE_ASSERT(count == 0);
  
  bool did_insert = mmap.insert(1, 111);
  HASHTABLE_ASSERT(did_insert);
  
  count = 0;
  for(auto v : mmap[1])
    ++count;
    
  HASHTABLE_ASSERT(count == 1);
}

static void debug_test_multimap_multi() {
  MultiMap<int, int> mmap;
  
  bool did_insert = mmap.insert(1, 111);
  HASHTABLE_ASSERT(did_insert);
  did_insert = mmap.insert(1, 222);
  HASHTABLE_ASSERT(did_insert);
  did_insert = mmap.insert(1, 333);
  HASHTABLE_ASSERT(did_insert);
  did_insert = mmap.insert(1, 222);
  HASHTABLE_ASSERT(!did_insert);
  
  did_insert = mmap.insert(5, 123);
  HASHTABLE_ASSERT(did_insert);
  
  int count = 0;
  for(auto v : mmap[1])
    ++count;
    
  HASHTABLE_ASSERT(count == 3);
  
  int sum = 0;
  for(auto v : mmap[1])
    sum+= v;
  
  HASHTABLE_ASSERT(sum == 111 + 222 + 333);
}

static void debug_test_multimap_delete() {
  MultiMap<int, double> mmap;
  
  bool did_insert = mmap.insert(1, 3.14);
  HASHTABLE_ASSERT(did_insert);
  
  did_insert = mmap.insert(2, 5.0);
  HASHTABLE_ASSERT(did_insert);
  
  did_insert = mmap.insert(2, 6.0);
  HASHTABLE_ASSERT(did_insert);
  
  did_insert = mmap.insert(2, 7.0);
  HASHTABLE_ASSERT(did_insert);
  
  bool did_remove = mmap.remove(2, 6.0);
  HASHTABLE_ASSERT(did_remove);
  
  double sum = 0;
  for(auto v : mmap[2])
    sum+= v;
  
  HASHTABLE_ASSERT(sum == 5.0 + 7.0);
}

#endif // NDEBUG
