#ifndef __BOXES__STYLEBOX_H__
#define __BOXES__STYLEBOX_H__

#include <boxes/ownerbox.h>


namespace richmath {
  class AbstractStyleBox: public OwnerBox {
    public:
      explicit AbstractStyleBox(MathSequence *content = 0);
      
      virtual void resize(Context *context);
      virtual void paint(Context *context);
      
      virtual void colorize_scope(SyntaxState *state);
      
      virtual Box *move_logical(
        LogicalDirection  direction,
        bool              jumping,
        int              *index);
        
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index);
        
      virtual Box *mouse_selection(
        float  x,
        float  y,
        int   *start,
        int   *end,
        bool  *was_inside_start);
        
    protected:
      void paint_or_resize(Context *context, bool paint);
      
    private:
      bool show_auto_styles;
  };
  
  class ExpandableAbstractStyleBox: public AbstractStyleBox {
    public:
      explicit ExpandableAbstractStyleBox(MathSequence *content = 0)
        : AbstractStyleBox(content)
      {
      }
      
      virtual bool expand(const BoxSize &size);
  };
  
  class StyleBox: public ExpandableAbstractStyleBox {
    public:
      explicit StyleBox(MathSequence *content = 0);
      
      // Box::try_create<StyleBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_STYLEBOX); }
      virtual Expr to_pmath(int flags);
      
      virtual bool changes_children_style() { return true; }
  };
  
  class TagBox: public ExpandableAbstractStyleBox {
    public:
      explicit TagBox(MathSequence *content = 0);
      TagBox(MathSequence *content, Expr _tag);
      
      // Box::try_create<TagBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, int opts);
      
      virtual void resize(Context *context);
      
      virtual Expr to_pmath_symbol() { return Symbol(PMATH_SYMBOL_TAGBOX); }
      virtual Expr to_pmath(int flags);
      
    public:
      Expr tag;
  };
}

#endif // __BOXES__STYLEBOX_H__
