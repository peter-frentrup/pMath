#ifndef RICHMATH__BOXES__SUBSUPERSCRIPTBOX_H__INCLUDED
#define RICHMATH__BOXES__SUBSUPERSCRIPTBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class SubsuperscriptBox final : public Box {
    protected:
      virtual ~SubsuperscriptBox();
    public:
      SubsuperscriptBox();
      SubsuperscriptBox(MathSequence *sub, MathSequence *super);
      
      // Box::try_create<SubsuperscriptBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *subscript() {   return _subscript; }
      MathSequence *superscript() { return _superscript; }
      
      virtual Box *item(int i) override;
      virtual int count() override;
      
      virtual int child_script_level(int index, const int *opt_ambient_script_level) final override;
      virtual void resize(Context &context) override;
      void stretch(Context &context, const BoxSize &base);
      void adjust_x(
        Context           &context,
        uint16_t           base_char,
        const GlyphInfo   &base_info);
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override;
      
      void complete();
      
      virtual Expr to_pmath_symbol() override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrix) override;
        
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
    
    private:
      MathSequence *_subscript;
      MathSequence *_superscript;
      
      Vector2F _subscript_offset;
      Vector2F _superscript_offset;
      float em;
  };
}

#endif // RICHMATH__BOXES__SUBSUPERSCRIPTBOX_H__INCLUDED
