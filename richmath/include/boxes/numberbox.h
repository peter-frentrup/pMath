#ifndef RICHMATH__BOXES__NUMBERBOX_H__INCLUDED
#define RICHMATH__BOXES__NUMBERBOX_H__INCLUDED

#include <boxes/ownerbox.h>

extern pmath_symbol_t richmath_FE_NumberBox;

namespace richmath {
  struct PositionInRange {
    int pos;
    int start;
    int end;
    
    PositionInRange(int _pos, int _start, int _end)
      : pos(_pos),
        start(_start),
        end(_end)
    {
    }
    
    bool is_valid() { return start <= pos && pos <= end; }
  };
  
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
      
      bool is_number_part(Box *box);
      PositionInRange selection_to_string_index(String number, Box *sel, int index);
      Box *string_index_to_selection(String number, int char_index, int *selection_index);
      
    private:
      String        _number;
      MathSequence *_base;
      MathSequence *_radius_base;
      MathSequence *_radius_exponent;
      MathSequence *_exponent;
  };
}

#endif /* RICHMATH__BOXES__NUMBERBOX_H__INCLUDED */
