#ifndef RICHMATH__BOXES__UNDEROVERSCRIPTBOX_H__INCLUDED
#define RICHMATH__BOXES__UNDEROVERSCRIPTBOX_H__INCLUDED

#include <boxes/box.h>


namespace richmath {
  class MathSequence;
  
  class UnderoverscriptBox final : public Box {
      using BaseClass = Box;
    protected:
      virtual ~UnderoverscriptBox();
    public:
      explicit UnderoverscriptBox();
      explicit UnderoverscriptBox(MathSequence *base, MathSequence *under, MathSequence *over);
      
      // Box::try_create<UnderoverscriptBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      MathSequence *base() {        return _base; }
      MathSequence *underscript() { return _underscript; }
      MathSequence *overscript() {  return _overscript; }
      
      virtual Box *item(int i) override;
      virtual int count() override;
      
      virtual int child_script_level(int index, const int *opt_ambient_script_level) final override;
      virtual void resize(Context &context) override;
      void after_items_resize(Context &context);
      virtual void colorize_scope(SyntaxState &state) override;
      virtual void paint(Context &context) override;
      
      virtual Box *remove(int *index) override;
      
      void complete();
      
      virtual Expr to_pmath_symbol() override;
      virtual Expr to_pmath(BoxOutputFlags flags) override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual void child_transformation(
        int             index,
        cairo_matrix_t *matrixn) override;
    
    protected:
      enum {
        OverscriptIsStretchedBit = BaseClass::NumFlagsBits,
        UnderscriptIsStretchedBit,
        
        NumFlagsBits
      };
      static_assert(NumFlagsBits <= MaximumFlagsBits, "");
      
      bool overscript_is_stretched() {        return get_flag(OverscriptIsStretchedBit); }
      void overscript_is_stretched(bool value) {  change_flag(OverscriptIsStretchedBit, value); }
      bool underscript_is_stretched() {       return get_flag(UnderscriptIsStretchedBit); }
      void underscript_is_stretched(bool value) { change_flag(UnderscriptIsStretchedBit, value); }
    
    private:
      MathSequence *_base;
      MathSequence *_underscript;
      MathSequence *_overscript;
      
      float    _base_offset_x;
      Vector2F _underscript_offset;
      Vector2F _overscript_offset;
  };
}

#endif // RICHMATH__BOXES__UNDEROVERSCRIPTBOX_H__INCLUDED
