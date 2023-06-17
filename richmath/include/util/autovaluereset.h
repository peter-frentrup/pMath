#ifndef RICHMATH__UTIL__AUTOVALUERESET_H__INCLUDED
#define RICHMATH__UTIL__AUTOVALUERESET_H__INCLUDED

namespace richmath {
  template<typename T>
  class AutoValueReset {
    public:
      AutoValueReset(T &_reference)
      : reference(_reference),
        old_value(_reference)
      {
      }
      
      ~AutoValueReset() {
        reference = old_value;
      }
      
    private:
      AutoValueReset(const AutoValueReset<T> &) = delete;
      const AutoValueReset<T> &operator=(const AutoValueReset<T> &) = delete;
      
    private:
      T &reference;
      T old_value;
  };
}

#endif // RICHMATH__UTIL__AUTOVALUERESET_H__INCLUDED
