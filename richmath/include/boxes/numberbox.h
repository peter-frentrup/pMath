#ifndef __BOXES__NUMBERBOX_H__
#define __BOXES__NUMBERBOX_H__

#include <boxes/ownerbox.h>

extern pmath_symbol_t richmath_FE_NumberBox;

namespace richmath {
  class NumberBox: public OwnerBox {
      friend class NumberBoxImpl;
    public:
      NumberBox();
      explicit NumberBox(String number);
      
      // Box::try_create<NumberBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual bool edit_selection(Context *context) override;
      
      virtual void resize(Context *context) override;
      virtual void colorize_scope(SyntaxState *state) override {}
      virtual void paint(Context *context) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      static Expr prepare_boxes(Expr boxes);
      
    private:
      String        _number;
      MathSequence *_base;
      MathSequence *_radius_base;
      MathSequence *_radius_exponent;
      MathSequence *_exponent;
  };
}

#endif /* __BOXES__NUMBERBOX_H__ */
