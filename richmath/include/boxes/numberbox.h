#ifndef __BOXES__NUMBERBOX_H__
#define __BOXES__NUMBERBOX_H__

#include <boxes/ownerbox.h>


namespace richmath {
  class NumberBox: public OwnerBox {
    public:
      NumberBox();
      explicit NumberBox(String number);
      
      // Box::try_create<NumberBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts) override;
      
      virtual bool edit_selection(Context *context) override;
      
      virtual void resize(Context *context) override;
      virtual void colorize_scope(SyntaxState *state) override {}
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(int flags) override;
      
      static Expr prepare_boxes(Expr boxes);
      
    private:
      void set_number(String n);
      
    private:
      String        _number;
      MathSequence *_base;
      MathSequence *_exponent;
      
      int _numstart;
      int _numend;
      int _expstart;
  };
}

#endif /* __BOXES__NUMBERBOX_H__ */
