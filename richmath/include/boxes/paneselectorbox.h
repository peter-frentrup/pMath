#ifndef RICHMATH__BOXES__PANESELECTORBOX_H__INCLUDED
#define RICHMATH__BOXES__PANESELECTORBOX_H__INCLUDED

#include <boxes/box.h>
#include <eval/dynamic.h>


namespace richmath {
  class MathSequence;
  
  class PaneSelectorBox : public Box {
    public:
      explicit PaneSelectorBox();
      virtual ~PaneSelectorBox();
      
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual Box *item(int i) override;
      virtual int count() override { return _panes.length(); }
      
      virtual void resize(Context *context) override;
      virtual void paint(Context *context) override;
      
      virtual void reset_style() override;
      
      virtual Box *remove(int *index) override;
      
      virtual Box *dynamic_to_literal(int *start, int *end) override;
      virtual void dynamic_updated() override;
      virtual void dynamic_finished(Expr info, Expr result) override;
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start) override;
      
      virtual bool edit_selection(Context *context) override;
    
    private:
      Expr to_literal();
    
    private:
      Array<Expr>          _cases;
      Array<MathSequence*> _panes;
      Dynamic              _dynamic;
      int                  _current_selection;
      bool                 _must_update      : 1;
  };
}

#endif // RICHMATH__BOXES__PANESELECTORBOX_H__INCLUDED
