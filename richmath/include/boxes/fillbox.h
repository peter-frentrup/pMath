#ifndef RICHMATH__BOXES__FILLBOX_H__INCLUDED
#define RICHMATH__BOXES__FILLBOX_H__INCLUDED

#include <boxes/ownerbox.h>


namespace richmath {
  class FillBox final : public OwnerBox {
      using base = OwnerBox;
    protected:
      virtual ~FillBox();
    public:
      explicit FillBox(AbstractSequence *content = nullptr);
      
      // Box::try_create<FillBox>(expr, opts);
      virtual bool try_load_from_object(Expr expr, BoxInputFlags opts) override;
      
      virtual bool expand(Context &context, const BoxSize &size) override;
      virtual void paint_content(Context &context) override;
      
      virtual Expr to_pmath_symbol() override;
      
      virtual Box *move_vertical(
        LogicalDirection  direction,
        float            *index_rel_x,
        int              *index,
        bool              called_from_child) override;
        
      virtual VolatileSelection mouse_selection(Point pos, bool *was_inside_start) override;
        
      virtual bool request_repaint(const RectangleF &rect) override;
      virtual bool visible_rect(RectangleF &rect, Box *top_most) override;
      
      virtual float fill_weight() override { return _weight; }
    
    protected:
      virtual Expr to_pmath_impl(BoxOutputFlags flags) override;
      virtual void resize_default_baseline(Context &context) override;
      
      virtual DefaultStyleOptionOffsets get_default_styles_offset() override { return DefaultStyleOptionOffsets::FillBox; }
    
    private:
      float _weight;
  };
}

#endif // RICHMATH__BOXES__FILLBOX_H__INCLUDED
