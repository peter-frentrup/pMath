#ifndef RICHMATH__BOXES__NUMBERBOX_H__INCLUDED
#define RICHMATH__BOXES__NUMBERBOX_H__INCLUDED

#include <boxes/ownerbox.h>

extern pmath_symbol_t richmath_FE_NumberBox;

namespace richmath {
  struct PositionInRange {
    int pos;
    Interval<int> range;
    
    PositionInRange(int _pos, Interval<int> _range)
      : pos(_pos),
        range(_range)
    {
    }
    
    PositionInRange(int _pos, int _start, int _end)
      : pos(_pos),
        range(_start, _end)
    {
    }
    
    static PositionInRange relative(Interval<int> range, int rel_pos) {
      return { range.from + rel_pos, range };
    }
    
    bool is_valid() { return range.contains(pos); }
  };
  
  class NumberBox final : public OwnerBox {
      using base = OwnerBox;
      class Impl;
    public:
      NumberBox();
      explicit NumberBox(String number);
      
      // Box::try_create<NumberBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual MathSequence *as_inline_span() override;
      
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override;
      
      virtual void colorize_scope(SyntaxState &state) override {}
      virtual void paint(Context &context) override;
      virtual void resize_inline(Context &context) override;
      
      virtual Expr to_pmath_symbol() override { return Expr(); }
      
      static Expr prepare_boxes(Expr boxes);
      virtual void dynamic_updated() override;
      
      bool is_number_part(Box *box);
      PositionInRange selection_to_string_index(String number, Box *sel, int index);
      Box *string_index_to_selection(String number, int char_index, int *selection_index);
      
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
      virtual void resize_default_baseline(Context &context) override;
      
      bool use_auto_formatting() {       return get_flag(UseAutoFormattingBit); }
      void use_auto_formatting(bool value) { change_flag(UseAutoFormattingBit, value); }
    
    protected:
      enum {
        UseAutoFormattingBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
    private:
      String        _number;
      MathSequence *_base;
      MathSequence *_radius_base;
      MathSequence *_radius_exponent;
      MathSequence *_exponent;
  };
}

#endif /* RICHMATH__BOXES__NUMBERBOX_H__INCLUDED */
