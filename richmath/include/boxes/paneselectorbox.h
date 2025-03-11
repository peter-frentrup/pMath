#ifndef RICHMATH__BOXES__PANESELECTORBOX_H__INCLUDED
#define RICHMATH__BOXES__PANESELECTORBOX_H__INCLUDED

#include <boxes/box.h>
#include <eval/dynamic.h>


namespace richmath {
  class MathSequence;
  
  class PaneSelectorBox final : public Box {
      using base = Box;
    protected:
      virtual ~PaneSelectorBox();
    public:
      explicit PaneSelectorBox();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Box *item(int i) override;
      virtual int count() override { return _panes.length(); }
      
      virtual void resize(Context &context) override;
      virtual void paint(Context &context) override;
      
      virtual void reset_style() override;
      
      virtual Box *remove(int *index) override;
      
      virtual VolatileSelection dynamic_to_literal(int start, int end) override;
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
      virtual Expr to_pmath_symbol() override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(int index, cairo_matrix_t *matrix) override;
      
      virtual bool edit_selection(SelectionReference &selection, EditAction action) override;
      
      int current_selection() { return _current_selection; }
      
    private:
      Expr to_literal();
    
    protected:
      enum {
        MustUpdateBit = base::NumFlagsBits,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool must_update() {       return get_flag(MustUpdateBit); }
      void must_update(bool value) { change_flag(MustUpdateBit, value); }
    
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::PaneSelectorBox; }

    private:
      Array<Expr>          _cases;
      Array<MathSequence*> _panes;
      Dynamic              _dynamic;
      Vector2F             _current_offset;
      int                  _current_selection;
  };
}

#endif // RICHMATH__BOXES__PANESELECTORBOX_H__INCLUDED
