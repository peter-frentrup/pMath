#include <util/semaphore.h>

#include <cerrno>
#include <cmath>


using namespace richmath;

//{ class Semaphore ...

Semaphore::Semaphore()
: Shareable()
{
  #ifdef _WIN32
    _value = CreateSemaphore(0,0,0x7FFFFFFF,0);
  #else
    sem_init(&_value, 0, 0);
  #endif
}

Semaphore::~Semaphore(){
  #ifdef _WIN32
    CloseHandle(_value);
  #else
    sem_destroy(&_value);
  #endif
}

void Semaphore::post(){
  #ifdef _WIN32
    ReleaseSemaphore(_value, 1, 0);
  #else
    sem_post(&_value);
  #endif
}

void Semaphore::wait(){
  #ifdef _WIN32
    WaitForSingleObject(_value, INFINITE);
  #else
    errno = 0;
    while(sem_wait(&_value) == -1 && errno == EINTR){
    }
  #endif
}

bool Semaphore::timed_wait(double seconds){
  #ifdef _WIN32
  {
    unsigned int ms = INFINITE;
    if(seconds * 1000 < Infinity && (int)(seconds * 1000) > 0)
      ms = (unsigned int)(seconds * 1000);

    return WaitForSingleObject(_value, ms) != WAIT_TIMEOUT;
  }
  #else
  {
    if(seconds < Infinity && (long)seconds >= 0){
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec+= (long)floor(seconds);//ms / 1000;
      ts.tv_nsec+= (long)((seconds - ts.tv_sec) * 1000000);//(ms % 1000) * 1000000;
      errno = 0;
      while(sem_wait(&_value) == -1 && errno == EINTR){
      }

      return errno != ETIMEDOUT;
    }
    else{
      errno = 0;
      while(sem_wait(&_value) == -1 && errno == EINTR){
      }

      return true;
    }
  }
  #endif
}

//} ... class Semaphore
