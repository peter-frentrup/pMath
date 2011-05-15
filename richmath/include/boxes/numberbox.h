#ifndef __BOXES__NUMBERBOX_H__
#define __BOXES__NUMBERBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class NumberBox: public OwnerBox {
    public:
      NumberBox(String number);
      
      virtual bool edit_selection(Context *context);
      
      virtual Expr to_pmath_symbol(){ return Expr(); }
      virtual Expr to_pmath(bool parseable){ return _number; }
      
      static Expr prepare_boxes(Expr boxes);
      
    private:
      void set_number(String n);
      
    private:
      String    _number;
      MathSequence *_exponent;
      
      int _numend;
      int _expstart;
  };
}

#endif /* __BOXES__NUMBERBOX_H__ */
