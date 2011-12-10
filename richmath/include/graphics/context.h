#ifndef __GRAPHICS__CONTEXT_H__
#define __GRAPHICS__CONTEXT_H__

#include <util/array.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>

#include <graphics/shapers.h>


namespace richmath {
  class Box;
  class GeneralSyntaxInfo;
  class Style;
  class Stylesheet;
  class SyntaxState;
  class WidgetBox;
  
  class SelectionReference {
    public:
      explicit SelectionReference(int _id = 0, int _start = 0, int _end = 0);
      
      Box *get();
      void set(Box *box, int _start, int _end);
      void reset() { set(0, 0, 0); }
      
      bool equals(Box *box, int _start, int _end) const;
      bool equals(const SelectionReference &other) const;
      
      bool operator==(const SelectionReference &other) const {
        return other.id == id && other.start == start && other.end == end;
      }
      
      bool operator!=(const SelectionReference &other) const {
        return !(*this == other);
      }
      
    public:
      int id;
      int start;
      int end;
  };
  
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
        int    color,
        float  radius,
        float  dx,
        float  dy);
        
      void draw_with_text_shadows(Box *box, Expr shadows);
      
    public:
      Canvas *canvas; // not owned
      
      float width;
      float section_content_window_width;
      float sequence_unfilled_width;
      
      float last_cursor_x[2];
      float last_cursor_y[2];
      
      String    fontname;
      FontStyle fontstyle;
      
      SharedPtr<TextShaper> text_shaper;
      SharedPtr<MathShaper> math_shaper;
      
      SelectionReference  selection;
      SelectionReference  old_selection; // cursor is not drawn if selection == old_selection
      int                 cursor_color;
      
      SharedPtr<GeneralSyntaxInfo> syntax;
      
      uint16_t multiplication_sign;
      
      bool show_auto_styles;
      bool show_string_characters;
      bool math_spacing;
      bool smaller_fraction_parts;
      bool single_letter_italics;
      bool boxchar_fallback_enabled;
      
      int          script_indent;
      float        script_size_min;
      Array<float> script_size_multis;
      
      SharedPtr<Stylesheet> stylesheet;
      
      int mouseover_box_id;
      int clicked_box_id;
      
      bool active;
  };
  
  class ContextState {
    public:
      explicit ContextState(Context *context): ctx(context) {}
      
      // does not change the color:
      void begin(SharedPtr<Style> style);
      
      void end();
      
    public:
      Context *ctx;
      
      // always set in begin():
      int                   old_cursor_color;
      int                   old_color;
      float                 old_fontsize;
      float                 old_width;
      SharedPtr<MathShaper> old_math_shaper;
      SharedPtr<TextShaper> old_text_shaper;
      bool                  old_math_spacing;
      bool                  old_show_auto_styles;
      bool                  old_show_string_characters;
      
      // not always set:
      cairo_antialias_t     old_antialiasing;
      Array<float>          old_script_size_multis;
  };
}

#endif // __GRAPHICS__CONTEXT_H__
