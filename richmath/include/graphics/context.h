#ifndef RICHMATH__GRAPHICS__CONTEXT_H__INCLUDED
#define RICHMATH__GRAPHICS__CONTEXT_H__INCLUDED


#include <syntax/syntax-state.h>

#include <util/array.h>
#include <util/pmath-extra.h>
#include <util/selections.h>
#include <util/sharedptr.h>

#include <graphics/paint-hook.h>
#include <graphics/ot-font-reshaper.h>
#include <graphics/syntax-styles.h>

#include <functional>


namespace richmath {
  class StyleData;
  class Stylesheet;
  class SyntaxState;
  
  class Context final : public Base {
    public:
      Context();
      
      void draw_error_rect(const RectangleF &rect) { draw_error_rect(rect.left(), rect.top(), rect.right(), rect.bottom()); }
      void draw_error_rect(
        float x1,
        float y1,
        float x2,
        float y2);
        
      void draw_selection_path();
      
      float get_script_size(float oldem);
      
      void set_script_size_multis(Expr expr);
      
      using PaintCallback = std::function<void(Context&)>;
      void draw_text_shadow(
             PaintCallback     painter,
             const RectangleF &region, 
             Color             color, 
             float             radius, 
             Vector2F          offset);
      
      bool draw_text_shadows(PaintCallback painter, const RectangleF &region, Expr shadows);
      void draw_with_text_shadows(Box *box, Expr shadows);
      
      void for_each_selection_at(    Box *box, std::function<void(const VolatileSelection&)> func);
      void for_each_selection_inside(Box *box, std::function<void(const VolatileSelection&)> func);
      void for_each_selection(                 std::function<void(const VolatileSelection&)> func);
      
    public:
      bool has_canvas() { return canvas_ptr != nullptr; }
      Canvas &canvas() { return *canvas_ptr; }
      Canvas *canvas_ptr; // not owned
      
      template<typename F>
      void with_canvas(Canvas &new_canvas, F func) {
        Canvas *old_canvas = canvas_ptr;
        canvas_ptr = &new_canvas;
        func();
        canvas_ptr = old_canvas;
      }
      
      PaintHookManager pre_paint_hooks;
      PaintHookManager post_paint_hooks;
      
      float width;
      float section_content_window_width;
      float sequence_unfilled_width;
      
      Point last_cursor_pos[2];
      
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
      
      int          script_level;
      float        script_size_min;
      Array<float> script_size_multis;
      
      SharedPtr<Stylesheet> stylesheet;
      
      FrontEndReference mouseover_box_id;
      FrontEndReference clicked_box_id;
      
      bool show_auto_styles : 1;
      bool show_string_characters : 1;
      bool math_spacing : 1;
      bool single_letter_italics : 1;
      bool boxchar_fallback_enabled : 1;
      
      bool active : 1;
  };
  
  class ContextState {
      class Impl;
    public:
      explicit ContextState(Context &context): ctx(context) {}
      
      // does not change the color:
      void begin(SharedPtr<StyleData> style);
      
      void apply_layout_styles(SharedPtr<StyleData> style);
      
      // does not change the color:
      void apply_non_layout_styles(SharedPtr<StyleData> style);
      
      void end();
      
    public:
      Context &ctx;
      
      // always set in begin():
      Color                        old_cursor_color;
      Color                        old_color;
      int                          old_script_level;
      float                        old_fontsize;
      float                        old_width;
      SharedPtr<MathShaper>        old_math_shaper;
      SharedPtr<TextShaper>        old_text_shaper;
      SharedPtr<GeneralSyntaxInfo> old_syntax;
      
      bool                  old_math_spacing : 1;
      bool                  old_show_auto_styles : 1;
      bool                  old_show_string_characters : 1;
      bool                  old_suppress_output : 1;
      bool                  have_font_feature_set : 1;
      
      // not always set:
      cairo_antialias_t     old_antialiasing;
      Array<float>          old_script_size_multis;
      FontFeatureSet        old_font_feature_set;
  };
  
  class AutoCallPaintHooks: public Base {
    public:
      AutoCallPaintHooks(Box *box, Context &context)
        : Base(),
          _box{box},
          _context{context},
          _p0{_context.canvas().current_pos()}
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
        _context.pre_paint_hooks.run(_box, _context);
      }
      
      ~AutoCallPaintHooks() {
        _context.canvas().move_to(_p0);
        _context.post_paint_hooks.run(_box, _context);
      }
      
    private:
      Box     *_box;
      Context &_context;
      Point    _p0;
  };
}

#endif // RICHMATH__GRAPHICS__CONTEXT_H__INCLUDED
