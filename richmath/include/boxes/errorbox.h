#ifndef __BOXES__ERRORBOX_H__
#define __BOXES__ERRORBOX_H__

#include <boxes/box.h>

namespace richmath{  
  class ErrorBox: public Box{
    public:
      ErrorBox(const Expr object);
      ~ErrorBox();
      
      Box *item(int i){ return NULL; }
      int count(){ return 0; }
      
      void resize(Context *context);
      void paint(Context *context);
      
      Box *remove(int *index){ return this; }
      
      pmath_t to_pmath(bool parseable);
      
    private:
      Expr _object;
  };
}

#endif // __BOXES__ERRORBOX_H__
