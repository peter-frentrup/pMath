#include <util/base.h>

#include <cmath>
#include <cstdio>

#include <pmath-util/concurrency/atomic.h>

using namespace richmath;

void richmath::assert_failed() {
  fprintf(stderr, "\n\n\tASSERTION FAILED\n\n");
}

const double richmath::Infinity = HUGE_VAL;

//{ class Base ...

#ifndef NDEBUG
class ObjectCounter {
  public:
    ObjectCounter() {
      pmath_atomic_write_release(&count, 0);
    }
    ~ObjectCounter() {
      if(pmath_atomic_read_aquire(&count) != 0)
        printf("%d OBJECTS NOT FREED\n", (int)pmath_atomic_read_aquire(&count));
    }
    
    pmath_atomic_t count;
};

static ObjectCounter counter;
#endif

Base::Base() {
#ifndef NDEBUG
  (void)pmath_atomic_fetch_add(&counter.count, 1);
#endif
}

Base::~Base() {
#ifndef NDEBUG
  (void)pmath_atomic_fetch_add(&counter.count, -1);
#endif
}

//} ... class Base

//{ class Shareable ...

Shareable::Shareable()
  : Base()
{
  pmath_atomic_write_release(&_refcount, 1);
}

Shareable::~Shareable() {
  assert(pmath_atomic_read_aquire(&_refcount) == 0);
}

void Shareable::ref() const {
  (void)pmath_atomic_fetch_add(&_refcount, 1);
}

void Shareable::unref() const {
  if(pmath_atomic_fetch_add(&_refcount, -1) == 1)
    delete this;
}

//} ... class Shareable
