#ifndef RICHMATH__GRAPHICS__CONTEXT_H__INCLUDED
#define RICHMATH__GRAPHICS__CONTEXT_H__INCLUDED

#include <util/array.h>
#include <util/pmath-extra.h>
#include <util/selections.h>
#include <util/sharedptr.h>

#include <graphics/paint-hook.h>
#include <graphics/ot-font-reshaper.h>


namespace richmath {
  class GeneralSyntaxInfo;
  class Style;
  class Stylesheet;
  class SyntaxState;
  class WidgetBox;
  
  class Context: public Base {
    public:
      Context();
      
      void draw_error_rect(
        float x1,
        float y1,
        float x2,
        float y2);
        
      void draw_selection_path();
      
      float get_script_size(float oldem);
      
      void set_script_size_multis(Expr expr);
      
      void draw_text_shadow(
        Box   *box,
        Color  color,
        float  radius,
        float  dx,
        float  dy);
        
      void draw_with_text_shadows(Box *box, Expr shadows);
      
    public:
      Canvas *canvas; // not owned
      
      PaintHookManager pre_paint_hooks;
      PaintHookManager post_paint_hooks;
      
      float width;
      float section_content_window_width;
      float sequence_unfilled_width;
      
      float last_cursor_x[2];
      float last_cursor_y[2];
      
      String         fontname;
      FontStyle      fontstyle;
      FontFeatureSet fontfeatures;
      
      SharedPtr<TextShaper> text_shaper;
      SharedPtr<MathShaper> math_shaper;
      
      SelectionReference  selection;
      SelectionReference  old_selection; // cursor is not drawn if selection == old_selection
      Color               cursor_color;
      
      SharedPtr<GeneralSyntaxInfo> syntax;
      
      uint16_t multiplication_sign;
      
      int          script_indent;
      float        script_size_min;
      Array<float> script_size_multis;
      
      SharedPtr<Stylesheet> stylesheet;
      
      FrontEndReference mouseover_box_id;
      FrontEndReference clicked_box_id;
      
      bool show_auto_styles : 1;
      bool show_string_characters : 1;
      bool math_spacing : 1;
      bool smaller_fraction_parts : 1;
      bool single_letter_italics : 1;
      bool boxchar_fallback_enabled : 1;
      
      bool active : 1;
  };
  
  class ContextState {
    public:
      explicit ContextState(Context *context): ctx(context) {}
      
      // does not change the color:
      void begin(SharedPtr<Style> style);
      
      void apply_layout_styles(SharedPtr<Style> style);
      
      // does not change the color:
      void apply_non_layout_styles(SharedPtr<Style> style);
      
      void end();
      
    public:
      Context *ctx;
      
      // always set in begin():
      Color                 old_cursor_color;
      Color                 old_color;
      float                 old_fontsize;
      float                 old_width;
      SharedPtr<MathShaper> old_math_shaper;
      SharedPtr<TextShaper> old_text_shaper;
      
      bool                  old_math_spacing : 1;
      bool                  old_show_auto_styles : 1;
      bool                  old_show_string_characters : 1;
      bool                  have_font_feature_set : 1;
      
      // not always set:
      cairo_antialias_t     old_antialiasing;
      Array<float>          old_script_size_multis;
      FontFeatureSet        old_font_feature_set;
  };
  
  class AutoCallPaintHooks: public Base {
    public:
      AutoCallPaintHooks(Box *box, Context *context)
        : Base(),
          _box(box),
          _context(context)
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
        _context->canvas->current_pos(&_x0, &_y0);
        _context->pre_paint_hooks.run(_box, _context);
      }
      
      ~AutoCallPaintHooks() {
        _context->canvas->move_to(_x0, _y0);
        _context->post_paint_hooks.run(_box, _context);
      }
      
    private:
      float _x0, _y0;
      Box     *_box;
      Context *_context;
  };
}

#endif // RICHMATH__GRAPHICS__CONTEXT_H__INCLUDED
