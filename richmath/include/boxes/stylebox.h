#ifndef __BOXES__STYLEBOX_H__
#define __BOXES__STYLEBOX_H__

#include <boxes/ownerbox.h>

namespace richmath{
  class AbstractStyleBox: public OwnerBox{
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
  
  class ExpandableAbstractStyleBox: public AbstractStyleBox{
    public:
      explicit ExpandableAbstractStyleBox(MathSequence *content = 0)
      : AbstractStyleBox(content)
      {
      }
      
      virtual bool expand(const BoxSize &size);
  };
  
  class StyleBox: public ExpandableAbstractStyleBox{
    public:
      static StyleBox *create(Expr expr, int opts); // returns 0 on error
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_STYLEBOX); }
      virtual Expr to_pmath(bool parseable);
      
      virtual bool changes_children_style(){ return true; }
    
    protected:
      explicit StyleBox(MathSequence *content = 0);
  };
  
  class TagBox: public ExpandableAbstractStyleBox{
    public:
      static TagBox *create(Expr expr, int options);
      
      virtual void resize(Context *context);
      
      virtual Expr to_pmath_symbol(){ return Symbol(PMATH_SYMBOL_TAGBOX); }
      virtual Expr to_pmath(bool parseable);
      
    protected:
      TagBox();
      explicit TagBox(MathSequence *content);
      TagBox(MathSequence *content, Expr _tag);
      
    public:
      Expr tag;
  };
}

#endif // __BOXES__STYLEBOX_H__
