#include <util/rle-array.h>
#include <stdio.h>


using namespace richmath;

#ifndef NDEBUG
namespace {
  class Trace {
    public:
      explicit Trace(const char *s) : s{s} {
//        fprintf(stderr, "%s ...\n", s);
      }
      ~Trace() {
//        fprintf(stderr, "%s done\n", s);
      }
    private:
      const char *s;
  };
}

#define TRACE_FUNC()  Trace _trace_(__func__);

static void debug_test_default();
static void debug_test_reset_rest();
static void debug_test_reset_range();
static void debug_test_reset_range_aaaaaaa_to_aaAAAaa();
static void debug_test_reset_range_aaaaaaa_to_aaBBBaa();
static void debug_test_reset_range_aabbbaa_to_aaAAbaa();
static void debug_test_reset_range_aabbbaa_to_aAAAbaa();
static void debug_test_reset_range_aabbbaa_to_aaAAAaa();
static void debug_test_reset_range_aabbbaa_to_aabAAaa();
static void debug_test_reset_range_aabbbaa_to_aabAAAa();
static void debug_test_reset_range_aabbbaa_to_aAAAAAa();
static void debug_test_reset_range_aabbbaa_to_aabCCCa();
static void debug_test_reset_range_aabbbaa_to_CCCbbaa();

void richmath::debug_test_rle_array() {
  TRACE_FUNC();
  
  debug_test_default();
  debug_test_reset_rest();
  debug_test_reset_range();
}

void debug_test_default() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  ARRAY_ASSERT(arr.find(0).get()   == 0);
  ARRAY_ASSERT(arr.find(5).get()   == 0);
  ARRAY_ASSERT(arr.find(100).get() == 0);
  
  ARRAY_ASSERT(arr.groups.length() == 0);
}

static void debug_test_reset_rest() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(3);
  iter.reset_rest(55);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 55);
  ARRAY_ASSERT(arr.find(4).get() == 55);
  ARRAY_ASSERT(arr.find(5).get() == 55);
  ARRAY_ASSERT(arr.find(6).get() == 55);
  ARRAY_ASSERT(arr.groups.length() == 1);
  
  ++iter;
  iter.reset_rest(99);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 55);
  ARRAY_ASSERT(arr.find(4).get() == 99);
  ARRAY_ASSERT(arr.find(5).get() == 99);
  ARRAY_ASSERT(arr.find(6).get() == 99);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  ++iter;
  iter.reset_rest(99);
  ARRAY_ASSERT(arr.groups.length() == 2);
}

static void debug_test_reset_range() {
  TRACE_FUNC();
  
  debug_test_reset_range_aaaaaaa_to_aaAAAaa();
  debug_test_reset_range_aaaaaaa_to_aaBBBaa();
  debug_test_reset_range_aabbbaa_to_aaAAbaa();
  debug_test_reset_range_aabbbaa_to_aAAAbaa();
  debug_test_reset_range_aabbbaa_to_aaAAAaa();
  debug_test_reset_range_aabbbaa_to_aabAAaa();
  debug_test_reset_range_aabbbaa_to_aabAAAa();
  debug_test_reset_range_aabbbaa_to_aAAAAAa();
  debug_test_reset_range_aabbbaa_to_aabCCCa();
  debug_test_reset_range_aabbbaa_to_CCCbbaa();
}

static void debug_test_reset_range_aaaaaaa_to_aaAAAaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(0, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 0);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 0);
}

static void debug_test_reset_range_aaaaaaa_to_aaBBBaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
}

static void debug_test_reset_range_aabbbaa_to_aaAAbaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  iter.reset_range(0, 2);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
}

static void debug_test_reset_range_aabbbaa_to_aAAAbaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  arr.find(1).reset_range(0, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
}

static void debug_test_reset_range_aabbbaa_to_aaAAAaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  iter.reset_range(0, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 0);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 0);
}

static void debug_test_reset_range_aabbbaa_to_aabAAaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  ++iter;

  iter.reset_range(0, 2);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 0);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
}

static void debug_test_reset_range_aabbbaa_to_aabAAAa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  ++iter;

  iter.reset_range(0, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 0);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
}

static void debug_test_reset_range_aabbbaa_to_aAAAAAa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  arr.find(1).reset_range(0, 5);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 0);
  ARRAY_ASSERT(arr.find(3).get() == 0);
  ARRAY_ASSERT(arr.find(4).get() == 0);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 0);
}

static void debug_test_reset_range_aabbbaa_to_aabCCCa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  ++iter;

  iter.reset_range(2, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 2);
  ARRAY_ASSERT(arr.find(4).get() == 2);
  ARRAY_ASSERT(arr.find(5).get() == 2);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 3);
}

static void debug_test_reset_range_aabbbaa_to_CCCbbaa() {
  TRACE_FUNC();
  
  RleArray<int, ConstPredictor<int>> arr;
  
  auto iter = arr.find(2);
  iter.reset_range(1, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 0);
  ARRAY_ASSERT(arr.find(1).get() == 0);
  ARRAY_ASSERT(arr.find(2).get() == 1);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 2);
  
  arr.find(0).reset_range(2, 3);
  
  ARRAY_ASSERT(arr.find(0).get() == 2);
  ARRAY_ASSERT(arr.find(1).get() == 2);
  ARRAY_ASSERT(arr.find(2).get() == 2);
  ARRAY_ASSERT(arr.find(3).get() == 1);
  ARRAY_ASSERT(arr.find(4).get() == 1);
  ARRAY_ASSERT(arr.find(5).get() == 0);
  ARRAY_ASSERT(arr.find(6).get() == 0);
  ARRAY_ASSERT(arr.groups.length() == 3);
}

#endif
