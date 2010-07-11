#ifndef SEMAPHORE_H_INCLUDED
#define SEMAPHORE_H_INCLUDED

#ifdef _WIN32
  #include <Windows.h>
#else
  #include <sem.h>
#endif

#include <util/base.h>

namespace richmath{
  class Semaphore: public Shareable {
    public:
      Semaphore();
      virtual ~Semaphore();
      
      void post();
      void wait();
      bool timed_wait(double seconds); // timeout => false
      
    private:
    #ifdef _WIN32
      HANDLE _value;
    #else
      sem_t  _value;
    #endif
  };
}

#endif // SEMAPHORE_H_INCLUDED
