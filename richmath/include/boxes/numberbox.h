#ifndef __BOXES__NUMBERBOX_H__
#define __BOXES__NUMBERBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class NumberBox: public OwnerBox {
    public:
      NumberBox(String number);
      
      virtual bool edit_selection(Context *context);
      
      virtual pmath_t to_pmath(bool parseable);
      
      static pmath_t prepare_boxes(pmath_t boxes);
      
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
