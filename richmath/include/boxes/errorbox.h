#ifndef __BOXES__ERRORBOX_H__
#define __BOXES__ERRORBOX_H__

#include <boxes/box.h>

namespace richmath{  
  class ErrorBox: public Box{
    public:
      ErrorBox(const Expr object);
      virtual ~ErrorBox();
      
      virtual Box *item(int i){ return NULL; }
      virtual int count(){ return 0; }
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual Box *remove(int *index){ return this; }
      
      virtual pmath_t to_pmath(bool parseable);
      
    private:
      Expr _object;
  };
}

#endif // __BOXES__ERRORBOX_H__
