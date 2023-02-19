#ifndef RICHMATH__UTIL__TEXT_GATHERING_H__INCLUDED
#define RICHMATH__UTIL__TEXT_GATHERING_H__INCLUDED

#include <util/pmath-extra.h>

namespace richmath {
  class Box;
  
  class SimpleTextGather {
      class Impl;
    public:
      explicit SimpleTextGather(int maxlen);
      
      void append(Box *box);
      
      void append_space();
      void append_text(String str);
      void append_text(const char *str);
    
    public:
      String text;
      int max_length;
      bool skip_string_characters_in_math : 1;
  };
}

#endif // RICHMATH__UTIL__TEXT_GATHERING_H__INCLUDED
