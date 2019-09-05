#include <boxes/mathsequence.h>

#include <climits>
#include <cmath>

#include <boxes/buttonbox.h>
#include <boxes/checkboxbox.h>
#include <boxes/dynamicbox.h>
#include <boxes/dynamiclocalbox.h>
#include <boxes/errorbox.h>
#include <boxes/fillbox.h>
#include <boxes/fractionbox.h>
#include <boxes/framebox.h>
#include <boxes/gridbox.h>
#include <boxes/inputfieldbox.h>
#include <boxes/interpretationbox.h>
#include <boxes/numberbox.h>
#include <boxes/openerbox.h>
#include <boxes/ownerbox.h>
#include <boxes/panelbox.h>
#include <boxes/paneselectorbox.h>
#include <boxes/progressindicatorbox.h>
#include <boxes/radicalbox.h>
#include <boxes/radiobuttonbox.h>
#include <boxes/section.h>
#include <boxes/setterbox.h>
#include <boxes/sliderbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/templatebox.h>
#include <boxes/tooltipbox.h>
#include <boxes/transformationbox.h>
#include <boxes/underoverscriptbox.h>
#include <boxes/graphics/graphicsbox.h>

#include <eval/binding.h>
#include <eval/application.h>

#include <graphics/context.h>
#include <graphics/ot-font-reshaper.h>
#include <graphics/scope-colorizer.h>

#include <util/spanexpr.h>


#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))


using namespace richmath;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_CheckboxBox;
extern pmath_symbol_t richmath_System_ComplexStringBox;
extern pmath_symbol_t richmath_System_DynamicBox;
extern pmath_symbol_t richmath_System_DynamicLocalBox;
extern pmath_symbol_t richmath_System_FillBox;
extern pmath_symbol_t richmath_System_FractionBox;
extern pmath_symbol_t richmath_System_FrameBox;
extern pmath_symbol_t richmath_System_GraphicsBox;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_InputFieldBox;
extern pmath_symbol_t richmath_System_InterpretationBox;
extern pmath_symbol_t richmath_System_OpenerBox;
extern pmath_symbol_t richmath_System_OverscriptBox;
extern pmath_symbol_t richmath_System_PanelBox;
extern pmath_symbol_t richmath_System_PaneSelectorBox;
extern pmath_symbol_t richmath_System_ProgressIndicatorBox;
extern pmath_symbol_t richmath_System_RadicalBox;
extern pmath_symbol_t richmath_System_RadioButtonBox;
extern pmath_symbol_t richmath_System_RotationBox;
extern pmath_symbol_t richmath_System_SetterBox;
extern pmath_symbol_t richmath_System_SliderBox;
extern pmath_symbol_t richmath_System_SqrtBox;
extern pmath_symbol_t richmath_System_StyleBox;
extern pmath_symbol_t richmath_System_SubscriptBox;
extern pmath_symbol_t richmath_System_SubsuperscriptBox;
extern pmath_symbol_t richmath_System_SuperscriptBox;
extern pmath_symbol_t richmath_System_TagBox;
extern pmath_symbol_t richmath_System_TemplateBox;
extern pmath_symbol_t richmath_System_TemplateSlot;
extern pmath_symbol_t richmath_System_TooltipBox;
extern pmath_symbol_t richmath_System_TransformationBox;
extern pmath_symbol_t richmath_System_UnderscriptBox;
extern pmath_symbol_t richmath_System_UnderoverscriptBox;

static const float RefErrorIndictorHeight = 1 / 3.0f;

static const float UnderoverscriptOverhangCoverage = 0.75f;
/*
   A value of 1 would        A value of 0 would
   yield:                    yield:
    /   n       \                 n
    | .---.     |             / .---.     \
    |  \        |             |  \        |
    |  /    ... |             |  /    ... |
    | '---'     |             \ '---'     /
    \  i=1      /                i=1
*/

class ScanData {
  public:
    MathSequence *sequence;
    int current_box; // for box_at_index
    BoxOutputFlags flags;
    int start;
    int end;
};

namespace richmath {
  class BreakPositionWithPenalty {
    public:
      BreakPositionWithPenalty()
        : text_position(-1),
          prev_break_index(-1),
          penalty(Infinity)
      {
      }
      
      BreakPositionWithPenalty(int tpos, int prev, double pen)
        : text_position(tpos),
          prev_break_index(prev),
          penalty(pen)
      {}
      
    public:
      int text_position; // after that character is a break
      int prev_break_index;
      double penalty;
  };
  
  class MathSequenceImpl {
    private:
      MathSequence &self;
      
    public:
      MathSequenceImpl(MathSequence &_self)
        : self(_self)
      {
      }
      
      //{ loading/scanner helpers
    public:
      static pmath_bool_t subsuperscriptbox_at_index(int i, void *_data) {
        ScanData *data = (ScanData *)_data;
        
        int start = data->current_box;
        while(data->current_box < data->sequence->boxes.length()) {
          if(data->sequence->boxes[data->current_box]->index() == i) {
            auto b = dynamic_cast<SubsuperscriptBox*>(data->sequence->boxes[data->current_box]);
            return 0 != b;
          }
          ++data->current_box;
        }
        
        data->current_box = 0;
        while(data->current_box < start) {
          if(data->sequence->boxes[data->current_box]->index() == i)
            return 0 != dynamic_cast<SubsuperscriptBox *>(
                     data->sequence->boxes[data->current_box]);
          ++data->current_box;
        }
        
        return FALSE;
      }
      
      static pmath_string_t underoverscriptbox_at_index(int i, void *_data) {
        ScanData *data = (ScanData *)_data;
        
        int start = data->current_box;
        while(data->current_box < data->sequence->boxes.length()) {
          if(data->sequence->boxes[data->current_box]->index() == i) {
            if(auto box = dynamic_cast<UnderoverscriptBox *>(data->sequence->boxes[data->current_box]))
              return pmath_ref(box->base()->text().get_as_string());
          }
          ++data->current_box;
        }
        
        data->current_box = 0;
        while(data->current_box < start) {
          if(data->sequence->boxes[data->current_box]->index() == i) {
            if(auto box = dynamic_cast<UnderoverscriptBox *>(data->sequence->boxes[data->current_box]))
              return pmath_ref(box->base()->text().get_as_string());
          }
          ++data->current_box;
        }
        
        return PMATH_NULL;
      }
      
      static void syntax_error(pmath_string_t code, int pos, void *_data, pmath_bool_t err) {
        ScanData *data = (ScanData *)_data;
        
        if(!data->sequence->get_style(ShowAutoStyles))
          return;
          
        const uint16_t *buf = pmath_string_buffer(&code);
        int             len = pmath_string_length(code);
        
        if(err) {
          if(pos < data->sequence->glyphs.length()
              && data->sequence->glyphs.length() > pos) {
            data->sequence->glyphs[pos].style = GlyphStyleSyntaxError;
          }
        }
        else if(pos < len && buf[pos] == '\n') { // new line character interpreted as multiplication
          while(pos > 0 && buf[pos] == '\n')
            --pos;
            
          if(pos >= 0
              && pos < data->sequence->glyphs.length()
              && data->sequence->glyphs.length() > pos) {
            data->sequence->glyphs[pos].missing_after = true;
          }
        }
      }
      
      static pmath_t box_at_index(int i, void *_data) {
        ScanData *data = (ScanData *)_data;
        
        BoxOutputFlags flags = data->flags;
        if(has(flags, BoxOutputFlags::Parseable) && data->sequence->is_inside_string(i)) {
          flags -= BoxOutputFlags::Parseable;
        }
        
        if(i < data->start || data->end <= i)
          return PMATH_FROM_TAG(PMATH_TAG_STR0, 0); // PMATH_C_STRING("")
          
        int start = data->current_box;
        while(data->current_box < data->sequence->boxes.length()) {
          if(data->sequence->boxes[data->current_box]->index() == i)
            return data->sequence->boxes[data->current_box]->to_pmath(flags).release();
          ++data->current_box;
        }
        
        data->current_box = 0;
        while(data->current_box < start) {
          if(data->sequence->boxes[data->current_box]->index() == i)
            return data->sequence->boxes[data->current_box]->to_pmath(flags).release();
          ++data->current_box;
        }
        
        return PMATH_NULL;
      }
      
      static pmath_t add_debug_info(
        pmath_t                             token_or_span,
        const struct pmath_text_position_t *start,
        const struct pmath_text_position_t *end,
        void                               *_data
      ) {
        ScanData *data = (ScanData *)_data;
        
        if(data->end <= start->index || end->index <= data->start) {
          pmath_unref(token_or_span);
          return PMATH_FROM_TAG(PMATH_TAG_STR0, 0); // PMATH_C_STRING("")
        }
        
        if(pmath_is_string(token_or_span)) {
          if(data->start > start->index || data->end < end->index) {
            /* does not work with string tokens containing boxes */
          
            if(start->index <= data->start && data->end <= end->index) {
              token_or_span = pmath_string_part(
                       token_or_span,
                       data->start - start->index,
                       data->end - data->start);
            }
            else if(data->start <= start->index && start->index <= data->end) {
              token_or_span = pmath_string_part(
                       token_or_span,
                       0,
                       data->end - start->index);
            }
            else if(data->start <= end->index && end->index <= data->end) {
              token_or_span = pmath_string_part(
                       token_or_span,
                       data->start - start->index,
                       end->index - data->start);
            }
          }
        }
        
        if(!has(data->flags, BoxOutputFlags::WithDebugInfo))
          return token_or_span;
        
        if(!pmath_is_expr(token_or_span) && !pmath_is_string(token_or_span))
          return token_or_span;
        
        Expr debug_info = SelectionReference(data->sequence->id(), start->index, end->index).to_debug_info();
                            
        token_or_span = pmath_try_set_debug_info(
                          token_or_span,
                          debug_info.release());
                          
        return token_or_span;
      }
      
      static pmath_t remove_null_tokens(pmath_t boxes) {
        while(true) {
          if(pmath_is_expr_of(boxes, PMATH_SYMBOL_LIST)) {
            size_t first = 1;
            size_t length = pmath_expr_length(boxes);
            size_t last = length;
            
            for(; last >= first; --last) {
              pmath_t item = pmath_expr_get_item(boxes, last);
              bool is_null_token = pmath_is_str0(item);
              pmath_unref(item);
              if(!is_null_token)
                break;
            }
            for(; first <= last; ++first) {
              pmath_t item = pmath_expr_get_item(boxes, first);
              bool is_null_token = pmath_is_str0(item);
              pmath_unref(item);
              if(!is_null_token)
                break;
            }
            
            if(first > last) {
              pmath_unref(boxes);
              return PMATH_FROM_TAG(PMATH_TAG_STR0, 0);
            }
            
            if(first == last) {
              pmath_t item = pmath_expr_get_item(boxes, first);
              pmath_unref(boxes);
              boxes = item;
              continue;
            }
            
            if(first == 1 && last == length)
              return boxes;
            
            if(first < last) {
              pmath_t items = pmath_expr_get_item_range(boxes, first, last - first + 1);
              pmath_unref(boxes);
              return items;
            }
          }
          return boxes;
        }
      }
      
      //}
      
      //{ vertical stretching
    private:
      void box_size(
        Context *context,
        int      pos,
        int      box,
        float    *a,
        float    *d
      ) {
        if(pos >= 0 && pos < self.glyphs.length()) {
          const uint16_t *buf = self.str.buffer();
          if(buf[pos] == PMATH_CHAR_BOX) {
            if(box < 0)
              box = self.get_box(pos);
              
            self.boxes[box]->extents().bigger_y(a, d);
          }
          else if(self.glyphs[pos].is_normal_text)
            context->text_shaper->vertical_glyph_size(
              context,
              buf[pos],
              self.glyphs[pos],
              a,
              d);
          else
            context->math_shaper->vertical_glyph_size(
              context,
              buf[pos],
              self.glyphs[pos],
              a,
              d);
        }
      }
      
    public:
      void boxes_size(
        Context *context,
        int      start,
        int      end,
        float    *a,
        float    *d
      ) {
        int box = -1;
        const uint16_t *buf = self.str.buffer();
        for(int i = start; i < end; ++i) {
          if(buf[i] == PMATH_CHAR_BOX) {
            if(box < 0) {
              do {
                ++box;
              } while(self.boxes[box]->index() < i);
            }
            self.boxes[box++]->extents().bigger_y(a, d);
          }
          else if(self.glyphs[i].is_normal_text) {
            context->text_shaper->vertical_glyph_size(
              context,
              buf[i],
              self.glyphs[i],
              a,
              d);
          }
          else {
            context->math_shaper->vertical_glyph_size(
              context,
              buf[i],
              self.glyphs[i],
              a,
              d);
          }
        }
      }
      
      void caret_size(
        Context *context,
        int      pos,
        int      box,
        float    *a,
        float    *d
      ) {
        if(self.glyphs.length() > 0) {
          box_size(context, pos - 1, box - 1, a, d);
          box_size(context, pos,     box,     a, d);
        }
        else {
          *a = self._extents.ascent;
          *d = self._extents.descent;
        }
      }
      
      //}
      
      //{ basic sizing
    public:
      void resize_span(
        Context *context,
        Span     span,
        int     *pos,
        int     *box
      ) {
        if(!span) {
          if(self.str[*pos] == PMATH_CHAR_BOX) {
            self.boxes[*box]->resize(context);
            
            self.glyphs[*pos].right = self.boxes[*box]->extents().width;
            self.glyphs[*pos].composed = 1;
            ++*box;
            ++*pos;
            return;
          }
          
          int next  = *pos;
          while(next < self.glyphs.length() && !self.spans.is_token_end(next))
            ++next;
            
          if(next < self.glyphs.length())
            ++next;
            
          const uint16_t *buf = self.str.buffer();
          
          if(context->math_spacing) {
            context->math_shaper->decode_token(
              context,
              next - *pos,
              buf + *pos,
              self.glyphs.items() + *pos);
          }
          else {
            context->text_shaper->decode_token(
              context,
              next - *pos,
              buf + *pos,
              self.glyphs.items() + *pos);
              
            for(int i = *pos; i < next; ++i) {
              if(self.glyphs[i].index) {
                self.glyphs[i].is_normal_text = 1;
              }
              else {
                context->math_shaper->decode_token(
                  context,
                  1,
                  buf + i,
                  self.glyphs.items() + i);
              }
            }
          }
          
          *pos = next;
          return;
        }
        
        if(!span.next() && self.str[*pos] == '"') {
          const uint16_t *buf = self.str.buffer();
          int end = span.end();
          if(!context->show_string_characters) {
            ++*pos;
            if(buf[end] == '"')
              --end;
          }
          else {
            context->math_shaper->decode_token(
              context,
              1,
              buf + *pos,
              self.glyphs.items() + *pos);
              
            if(buf[end] == '"')
              context->math_shaper->decode_token(
                context,
                1,
                buf + end,
                self.glyphs.items() + end);
          }
          
          bool old_math_styling = context->math_spacing;
          context->math_spacing = false;
          
          while(*pos <= end) {
            if(buf[*pos] == PMATH_CHAR_BOX) {
              self.boxes[*box]->resize(context);
              self.glyphs[*pos].right = self.boxes[*box]->extents().width;
              self.glyphs[*pos].composed = 1;
              ++*box;
              ++*pos;
            }
            else {
              int next = *pos;
              while(next <= end && !self.spans.is_token_end(next))
                ++next;
              ++next;
              
              if(!context->show_string_characters && buf[*pos] == '\\')
                ++*pos;
                
              context->text_shaper->decode_token(
                context,
                next - *pos,
                buf + *pos,
                self.glyphs.items() + *pos);
                
              for(int i = *pos; i < next; ++i) {
                self.glyphs[i].is_normal_text = 1;
              }
              
              *pos = next;
            }
          }
          
          context->math_spacing = old_math_styling;
          
          *pos = span.end() + 1;
        }
        else {
          resize_span(context, span.next(), pos, box);
          while(*pos <= span.end())
            resize_span(context, self.spans[*pos], pos, box);
        }
      }
      
    public:
      void stretch_span(
        Context *context,
        Span     span,
        int     *pos,
        int     *box,
        float    *core_ascent,
        float    *core_descent,
        float    *ascent,
        float    *descent
      ) {
        const uint16_t *buf = self.str.buffer();
        
        if(span) {
          int start = *pos;
          if(!span.next()) {
            uint16_t ch = buf[start];
            
            if(ch == '"') {
              for(; *pos <= span.end(); ++*pos) {
                if(buf[*pos] == PMATH_CHAR_BOX)
                  ++*box;
              }
              
              return;
            }
            
            if(ch == PMATH_CHAR_BOX) {
              auto underover = dynamic_cast<UnderoverscriptBox*>(self.boxes[*box]);
              if(underover && underover->base()->length() == 1)
                ch = underover->base()->str[0];
            }
            
            if(pmath_char_is_left(ch)) {
              float ca = 0;
              float cd = 0;
              float a = 0;
              float d = 0;
              
              ++*pos;
              while(*pos <= span.end() && !pmath_char_is_right(buf[*pos]))
                stretch_span(context, self.spans[*pos], pos, box, &ca, &cd, &a, &d);
                
              float overhang_a = (a - ca) * UnderoverscriptOverhangCoverage;
              float overhang_d = (d - cd) * UnderoverscriptOverhangCoverage;
              
              float new_ca = ca + overhang_a;
              float new_cd = cd + overhang_d;
              
              bool full_stretch = true;
              if(*pos <= span.end() && pmath_char_is_right(buf[*pos])) {
                if(ch == '{' && buf[*pos] == '}')
                  full_stretch = false;
                
                context->math_shaper->vertical_stretch_char(
                  context, new_ca, new_cd, full_stretch, buf[*pos], &self.glyphs[*pos]);
                  
                ++*pos;
              }
              
              context->math_shaper->vertical_stretch_char(
                context, new_ca, new_cd, full_stretch, buf[start], &self.glyphs[start]);
                
              if(*ascent < a)
                *ascent = a;
                
              if(*core_ascent < new_ca)
                *core_ascent = new_ca;
                
              if(*descent < d)
                *descent = d;
                
              if(*core_descent < new_cd)
                *core_descent = new_cd;
            }
            else if(pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)) {
              float a = 0;
              float d = 0;
              int startbox = *box;
              
              if(buf[start] == PMATH_CHAR_BOX) {
                assert(dynamic_cast<UnderoverscriptBox *>(self.boxes[startbox]) != 0);
                
                ++*pos;
                ++*box;
              }
              else {
                ++*pos;
                
                if( *pos < self.glyphs.length() &&
                    buf[*pos] == PMATH_CHAR_BOX)
                {
                  if(dynamic_cast<SubsuperscriptBox *>(self.boxes[startbox])) {
                    ++*box;
                    ++*pos;
                  }
                }
              }
              
              while(*pos <= span.end())
                stretch_span(context, self.spans[*pos], pos, box, &a, &d, ascent, descent);
                
              if(buf[start] == PMATH_CHAR_BOX) {
                auto underover = dynamic_cast<UnderoverscriptBox*>(self.boxes[startbox]);
                
                assert(underover != 0);
                
                context->math_shaper->vertical_stretch_char(
                  context,
                  a,
                  d,
                  true,
                  underover->base()->str[0],
                  &underover->base()->glyphs[0]);
                  
                context->math_shaper->vertical_glyph_size(
                  context,
                  underover->base()->str[0],
                  underover->base()->glyphs[0],
                  &underover->base()->_extents.ascent,
                  &underover->base()->_extents.descent);
                  
                underover->base()->_extents.width = underover->base()->glyphs[0].right;
                
                underover->after_items_resize(context);
                
                self.glyphs[start].right = underover->extents().width;
                
                underover->base()->extents().bigger_y(core_ascent, core_descent);
                underover->extents().bigger_y(ascent, descent);
              }
              else {
                context->math_shaper->vertical_stretch_char(
                  context,
                  a,
                  d,
                  true,
                  buf[start],
                  &self.glyphs[start]);
                  
                BoxSize size;
                context->math_shaper->vertical_glyph_size(
                  context,
                  buf[start],
                  self.glyphs[start],
                  &size.ascent,
                  &size.descent);
                  
                size.bigger_y(core_ascent, core_descent);
                size.bigger_y(ascent,      descent);
                
                if( start + 1 < self.glyphs.length() &&
                    buf[start + 1] == PMATH_CHAR_BOX)
                {
                  if(auto subsup = dynamic_cast<SubsuperscriptBox *>(self.boxes[startbox])) {
                    subsup->stretch(context, size);
                    subsup->extents().bigger_y(ascent, descent);
                    
                    subsup->adjust_x(context, buf[start], self.glyphs[start]);
                  }
                }
              }
              
              if(*core_ascent < a)
                *core_ascent = a;
              if(*core_descent < d)
                *core_descent = d;
            }
            else
              stretch_span(context, span.next(), pos, box, core_ascent, core_descent, ascent, descent);
          }
          else
            stretch_span(context, span.next(), pos, box, core_ascent, core_descent, ascent, descent);
            
          if( *pos <= span.end() &&
              buf[*pos] == '/' &&
              self.spans.is_token_end(*pos))
          {
            start = *pos;
            
            ++*pos;
            while(*pos <= span.end())
              stretch_span(context, self.spans[*pos], pos, box, core_ascent, core_descent, ascent, descent);
              
            context->math_shaper->vertical_stretch_char(
              context,
              *core_ascent  - 0.1 * self.em,
              *core_descent - 0.1 * self.em,
              true,
              buf[start],
              &self.glyphs[start]);
              
            BoxSize size;
            context->math_shaper->vertical_glyph_size(
              context,
              buf[start],
              self.glyphs[start],
              &size.ascent,
              &size.descent);
              
            size.bigger_y(core_ascent, core_descent);
            size.bigger_y(ascent,      descent);
          }
          
          while(*pos <= span.end() && (!pmath_char_is_left(buf[*pos]) || self.spans[*pos]))
            stretch_span(context, self.spans[*pos], pos, box, core_ascent, core_descent, ascent, descent);
            
          if(*pos < span.end()) {
            start = *pos;
            
            float ca = 0;
            float cd = 0;
            float a = 0;
            float d = 0;
            
            ++*pos;
            while(*pos <= span.end() && !pmath_char_is_right(buf[*pos]))
              stretch_span(context, self.spans[*pos], pos, box, &ca, &cd, &a, &d);
              
            float overhang_a = (a - ca) * UnderoverscriptOverhangCoverage;
            float overhang_d = (d - cd) * UnderoverscriptOverhangCoverage;
            
            float new_ca = ca + overhang_a;
            float new_cd = cd + overhang_d;
            
            if(*pos <= span.end() && pmath_char_is_right(buf[*pos])) {
              context->math_shaper->vertical_stretch_char(
                context, new_ca, new_cd, false, buf[*pos], &self.glyphs[*pos]);
                
              ++*pos;
            }
            
            context->math_shaper->vertical_stretch_char(
              context, new_ca, new_cd, false, buf[start], &self.glyphs[start]);
              
            if(*ascent < a)
              *ascent = a;
              
            if(*core_ascent < new_ca)
              *core_ascent = new_ca;
              
            if(*descent < d)
              *descent = d;
              
            if(*core_descent < new_cd)
              *core_descent = new_cd;
              
            while(*pos <= span.end()) {
              stretch_span(
                context,
                self.spans[*pos],
                pos,
                box,
                core_ascent,
                core_descent,
                ascent, descent);
            }
          }
          
          return;
        }
        
        if(buf[*pos] == PMATH_CHAR_BOX) {
          auto subsup = dynamic_cast<SubsuperscriptBox*>(self.boxes[*box]);
          
          if(subsup && *pos > 0) {
            if(buf[*pos - 1] == PMATH_CHAR_BOX) {
              subsup->stretch(context, self.boxes[*box - 1]->extents());
            }
            else {
              BoxSize size;
              
              context->math_shaper->vertical_glyph_size(
                context, buf[*pos - 1], self.glyphs[*pos - 1],
                &size.ascent, &size.descent);
                
              subsup->stretch(context, size);
              subsup->adjust_x(context, buf[*pos - 1], self.glyphs[*pos - 1]);
            }
            
            subsup->extents().bigger_y(ascent,      descent);
            subsup->extents().bigger_y(core_ascent, core_descent);
          }
          else {
            if(auto underover = dynamic_cast<UnderoverscriptBox *>(self.boxes[*box])) {
              uint16_t ch = 0;
              
              if(underover->base()->length() == 1)
                ch = underover->base()->text()[0];
                
              if( self.spans.is_operand_start(*pos) &&
                  (pmath_char_maybe_bigop(ch) || pmath_char_is_integral(ch)))
              {
                context->math_shaper->vertical_stretch_char(
                  context,
                  0,
                  0,
                  true,
                  underover->base()->str[0],
                  &underover->base()->glyphs[0]);
                  
                context->math_shaper->vertical_glyph_size(
                  context,
                  underover->base()->str[0],
                  underover->base()->glyphs[0],
                  &underover->base()->_extents.ascent,
                  &underover->base()->_extents.descent);
                  
                underover->base()->_extents.width = underover->base()->glyphs[0].right;
                
                underover->after_items_resize(context);
                
                self.glyphs[*pos].right = underover->extents().width;
              }
              
              underover->base()->extents().bigger_y(core_ascent, core_descent);
            }
            else
              self.boxes[*box]->extents().bigger_y(core_ascent, core_descent);
          }
          
          self.boxes[*box]->extents().bigger_y(ascent, descent);
          ++*box;
          ++*pos;
          return;
        }
        
        if( self.spans.is_operand_start(*pos) &&
            self.length() > 1 &&
            (pmath_char_maybe_bigop(buf[*pos]) || pmath_char_is_integral(buf[*pos])))
        {
          context->math_shaper->vertical_stretch_char(
            context,
            0,
            0,
            true,
            buf[*pos],
            &self.glyphs[*pos]);
            
          BoxSize size;
          context->math_shaper->vertical_glyph_size(
            context,
            buf[*pos],
            self.glyphs[*pos],
            &size.ascent,
            &size.descent);
            
          size.bigger_y(core_ascent, core_descent);
          size.bigger_y(ascent,      descent);
          
          ++*pos;
          return;
        }
        
        do {
          context->math_shaper->vertical_glyph_size(
            context, buf[*pos], self.glyphs[*pos], core_ascent, core_descent);
          ++*pos;
        } while(*pos < self.str.length() && !self.spans.is_token_end(*pos - 1));
        
        if(*ascent < *core_ascent)
          *ascent = *core_ascent;
        if(*descent < *core_descent)
          *descent = *core_descent;
      }
      
      //}
      
      //{ OpenType substitutions
    private:
      void substitute_glyphs(
        Context              *context,
        int                   start,
        int                   end,
        uint32_t              math_script_tag,
        uint32_t              math_language_tag,
        uint32_t              text_script_tag,
        uint32_t              text_language_tag,
        const FontFeatureSet &features
      ) {
        if(features.empty())
          return;
          
        const uint16_t *buf = self.str.buffer();
        cairo_text_extents_t cte;
        cairo_glyph_t        cg;
        cg.x = cg.y = 0;
        
        int run_start = start;
        while(run_start < end) {
          if( self.glyphs[run_start].composed ||
              buf[run_start] == PMATH_CHAR_BOX)
          {
            ++run_start;
            continue;
          }
          
          int next_run = run_start + 1;
          for(; next_run < end; ++next_run) {
            if(self.glyphs[next_run].composed || buf[run_start] == PMATH_CHAR_BOX)
              break;
              
            if(self.glyphs[run_start].fontinfo != self.glyphs[next_run].fontinfo)
              break;
            if(self.glyphs[run_start].slant != self.glyphs[next_run].slant)
              break;
            if(self.glyphs[run_start].is_normal_text != self.glyphs[next_run].is_normal_text)
              break;
          }
          
          uint32_t script_tag, language_tag;
          SharedPtr<TextShaper> shaper;
          
          if(self.glyphs[run_start].is_normal_text) {
            shaper = context->text_shaper;
            script_tag   = text_script_tag;
            language_tag = text_language_tag;
          }
          else {
            shaper = context->math_shaper;
            script_tag   = math_script_tag;
            language_tag = math_language_tag;
          }
          
          FontFace face = shaper->font(self.glyphs[run_start].fontinfo);
          FontInfo info(face);
          
          if(const auto gsub = info.get_gsub_table()) {
            static Array<OTFontReshaper::IndexAndValue> lookups;
            lookups.length(0);
            
            OTFontReshaper::get_lookups(
              gsub,
              script_tag,
              language_tag,
              features,
              &lookups);
              
            if(lookups.length() > 0) {
              context->canvas->set_font_face(face);
              
              static OTFontReshaper reshaper;
              
              reshaper.glyphs.length(    next_run - run_start);
              reshaper.glyph_info.length(next_run - run_start);
              
              reshaper.glyphs.length(0);
              reshaper.glyph_info.length(0);
              
              for(int i = run_start; i < next_run; ++i) {
                if(self.glyphs[i].index == IgnoreGlyph)
                  continue;
                  
                reshaper.glyphs.add(self.glyphs[i].index);
                reshaper.glyph_info.add(i);
              }
              
              reshaper.apply_lookups(gsub, lookups);
              
              assert(reshaper.glyphs.length() == reshaper.glyph_info.length());
              
              int len = reshaper.glyphs.length();
              int i = 0;
              while(i < len) {
                int i2 = i + 1;
                
                int pos = reshaper.glyph_info[i];
                
                if(i2 < len && reshaper.glyph_info[i2] == pos) {
                  // no room for "one to many" substitution
                  
                  ++i2;
                  while(i2 < len && reshaper.glyph_info[i2] == pos)
                    ++i2;
                    
                  i = i2;
                  continue;
                }
                
                int next;
                if(i2 < len)
                  next = reshaper.glyph_info[i2];
                else
                  next = next_run;
                  
                assert(pos < next);
                
                if(self.glyphs[pos].index != reshaper.glyphs[i]) {
                  cg.index = self.glyphs[pos].index = reshaper.glyphs[i];
                  
                  context->canvas->glyph_extents(&cg, 1, &cte);
                  cte.x_advance /= (next - pos);
                  self.glyphs[pos].right = cte.x_advance;
                  
                  for(int j = pos + 1; j < next; ++j) {
                    self.glyphs[j].right = cte.x_advance;
                    self.glyphs[j].index = IgnoreGlyph;
                  }
                }
                
                i = i2;
              }
            }
          }
          run_start = next_run;
        }
      }
      
    public:
      void apply_glyph_substitutions(Context *context) {
        if(context->fontfeatures.empty())
          return;
          
        int old_ssty_feature_value = context->fontfeatures.feature_value(FontFeatureSet::TAG_ssty);
        if(old_ssty_feature_value < 0)
          context->fontfeatures.set_feature(FontFeatureSet::TAG_ssty, context->script_indent);
          
        /* TODO: infer script ("math") and language ("dflt") from style/context.
         */
        
        substitute_glyphs(
          context,
          0,
          self.glyphs.length(),
          OTFontReshaper::SCRIPT_math,
          OTFontReshaper::LANG_dflt,
          OTFontReshaper::SCRIPT_latn, //OTFontReshaper::SCRIPT_DFLT
          OTFontReshaper::LANG_dflt,
          context->fontfeatures);
          
        substitute_glyphs(
          context,
          0,
          self.glyphs.length(),
          OTFontReshaper::SCRIPT_latn,
          OTFontReshaper::LANG_dflt,
          OTFontReshaper::SCRIPT_latn, //OTFontReshaper::SCRIPT_DFLT
          OTFontReshaper::LANG_dflt,
          context->fontfeatures);
          
        context->fontfeatures.set_feature(FontFeatureSet::TAG_ssty, old_ssty_feature_value);
      }
      
      //}
      
      //{ horizontal kerning/spacing
    private:
      class EnlargeSpace {
        private:
          MathSequence &self;
          Context *context;
          
          const uint16_t *buf;
          
        public:
          EnlargeSpace(MathSequence &_self, Context *_context)
            : self(_self),
              context(_context),
              buf(_self.str.buffer())
          {
          }
          
        private:
          void group_number_digits(int start, int end) {
            static const int min_int_digits  = 5;
            static const int min_frac_digits = 7;//INT_MAX;
            static const int int_group_size  = 3;
            static const int frac_group_size = 5;
            
            int decimal_point = end + 1;
            
            float half_space_width = self.em / 10; // total: em/5 = thin space
            
            for(int i = start; i <= end; ++i) {
              if(buf[i] < '0' || buf[i] > '9') {
                if(buf[i] == '^') // explicitly specified base
                  return;
                  
                if(buf[i] == '.' && i < decimal_point) {
                  decimal_point = i;
                  continue;
                }
                
                if(buf[i] == '`') { // precision control
                  if(i < decimal_point)
                    decimal_point = i;
                    
                  end = i - 1;
                  break;
                }
              }
            }
            
            if( int_group_size        >  0              &&
                decimal_point - start >= min_int_digits &&
                decimal_point - start >  int_group_size)
            {
              for(int i = decimal_point - int_group_size; i > start; i -= int_group_size) {
                self.glyphs[i - 1].right += half_space_width;
                
                self.glyphs[i].x_offset  += half_space_width;
                self.glyphs[i].right     += half_space_width;
              }
            }
            
            if( frac_group_size     >  0               &&
                end - decimal_point >= min_frac_digits &&
                end - decimal_point >  frac_group_size)
            {
              for(int i = decimal_point + frac_group_size; i < end; i += frac_group_size) {
                self.glyphs[i].right        += half_space_width;
                
                self.glyphs[i + 1].x_offset += half_space_width;
                self.glyphs[i + 1].right    += half_space_width;
              }
            }
          }
          
          bool slant_is_italic(int glyph_slant) {
            switch(glyph_slant) {
              case FontSlantPlain:
                return false;
              case FontSlantItalic:
                return true;
            }
            return context->math_shaper->get_style().italic;
          }
          
          void italic_correction(int token_end) {
            if(buf[token_end] == PMATH_CHAR_BOX)
              return;
              
            if(!slant_is_italic(self.glyphs[token_end].slant)) {
              if(!pmath_char_is_integral(buf[token_end]))
                return;
            }
            
            if( token_end + 1 == self.glyphs.length() ||
                !slant_is_italic(self.glyphs[token_end + 1].slant) ||
                buf[token_end + 1] == PMATH_CHAR_BOX ||
                pmath_char_is_integral(buf[token_end]))
            {
              float ital_corr = context->math_shaper->italic_correction(
                                  context,
                                  buf[token_end],
                                  self.glyphs[token_end]);
                                  
              ital_corr *= self.em;
              if(token_end + 1 < self.glyphs.length()) {
                self.glyphs[token_end + 1].x_offset += ital_corr;
                self.glyphs[token_end + 1].right += ital_corr;
              }
              else
                self.glyphs[token_end].right += ital_corr;
            }
          }
          
          void skip_subsuperscript(int &token_end, int &next_box_index) {
            while(token_end + 1 < self.glyphs.length() &&
                  buf[token_end + 1] == PMATH_CHAR_BOX &&
                  next_box_index < self.boxes.length())
            {
              while(next_box_index < self.boxes.length() &&
                    self.boxes[next_box_index]->index() <= token_end) {
                ++next_box_index;
              }
              
              if( next_box_index == self.boxes.length() ||
                  !dynamic_cast<SubsuperscriptBox *>(self.boxes[next_box_index]))
              {
                break;
              }
              
              ++token_end;
            }
          }
          
          void show_tab_character(int pos, bool in_string) {
            static uint16_t arrow = 0x21e2;//0x27F6;
            float width = 4 * context->canvas->get_font_size();
            
            if(context->show_auto_styles && !in_string) {
              context->math_shaper->decode_token(
                context,
                1,
                &arrow,
                &self.glyphs[pos]);
                
              self.glyphs[pos].x_offset = (width - self.glyphs[pos].right) / 2;
              
              self.glyphs[pos].style = GlyphStyleImplicit;
            }
            else {
              self.glyphs[pos].index = 0;
            }
            
            self.glyphs[pos].right = width;
          }
          
          void find_box_token(const uint16_t *&op, int &ii, int &ee, int &next_box_index) {
            assert(op == buf);
            assert(op[ii] == PMATH_CHAR_BOX);
            assert(ii <= ee);
            
            int i = ii;
            int e = ee;
            
            while(self.boxes[next_box_index]->index() < i)
              ++next_box_index;
              
            if(next_box_index < self.boxes.length()) {
              Box *tmp = self.boxes[next_box_index];
              while(tmp) {
                if(AbstractStyleBox *asb = dynamic_cast<AbstractStyleBox *>(tmp)) {
                  tmp = asb->content();
                  continue;
                }
                
                if(MathSequence *seq = dynamic_cast<MathSequence *>(tmp)) {
                  if(seq->length() == 1 && seq->count() == 1) {
                    tmp = seq->item(0);
                    continue;
                  }
                  
                  op = seq->text().buffer();
                  ii = 0;
                  ee = ii;
                  while( ee < seq->length() &&
                         !seq->span_array().is_token_end(ee))
                  {
                    ++ee;
                  }
                  
                  if(ee != seq->length() - 1) {
                    op = buf;
                    ii = i;
                    ee = e;
                  }
                  
                  break;
                }
                
                if(UnderoverscriptBox *uob = dynamic_cast<UnderoverscriptBox *>(tmp)) {
                  tmp = uob->base();
                  continue;
                }
                
//                if(FractionBox *fb = dynamic_cast<FractionBox *>(tmp)) {
//                  if(i > 0 && pmath_char_is_digit(buf[i - 1])) {
//
//                  }
//                }

                break;
              }
              
//              UnderoverscriptBox *underover = dynamic_cast<UnderoverscriptBox *>(tmp);
//              if(underover && underover->base()->length() > 0) {
//                op = underover->base()->text().buffer();
//                ii = 0;
//                ee = ii;
//                while( ee < underover->base()->length() &&
//                       !underover->base()->span_array().is_token_end(ee))
//                {
//                  ++ee;
//                }
//
//                if(ee != underover->base()->length() - 1) {
//                  op = buf;
//                  ii = i;
//                  ee = e;
//                }
//              }
            }
          }
          
          static pmath_token_t get_box_start_token(Box *box) {
            while(box) {
              if(AbstractStyleBox *asb = dynamic_cast<AbstractStyleBox *>(box)) {
                box = asb->content();
                continue;
              }
              
              if(UnderoverscriptBox *uob = dynamic_cast<UnderoverscriptBox *>(box)) {
                box = uob->base();
                continue;
              }
              
              if(NumberBox *nb = dynamic_cast<NumberBox *>(box)) {
                box = nb->content();
                continue;
              }
              
              if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
                if(seq->length() == 0)
                  break;
                  
                const uint16_t *buf = seq->text().buffer();
                if(buf[0] == PMATH_CHAR_BOX) {
                  box = seq->item(0);
                  continue;
                }
                
                return pmath_token_analyse(buf, 1, nullptr);
              }
              
              if(dynamic_cast<FractionBox *>(box))
                return PMATH_TOK_DIGIT;
                
              break;
            }
            
            return PMATH_TOK_NAME2;
          }
          
          int get_string_end(int pos) {
            Span span = self.spans[pos];
            if(!span)
              return pos + 1;
            for(;;) {
              Span next = span.next();
              if(!next)
                return span.end();
              span = next;
            }
          }
          
        public:
          void run() {
            if(context->script_indent > 0)
              return;
              
            int box = 0;
            int string_end = -1;
            bool in_alias = false;
            bool last_was_factor = false;
            //bool last_was_number = false;
            bool last_was_space  = false;
            bool last_was_left   = false;
            
            int e = -1;
            
            while(true) {
              int i = e += 1;
              if(i >= self.glyphs.length())
                break;
                
              while(e < self.glyphs.length() && !self.spans.is_token_end(e))
                ++e;
                
              italic_correction(e);
              skip_subsuperscript(e, box);
              
              if(buf[i] == '\t') {
                show_tab_character(i, i <= string_end);
                continue;
              }
              
              if(string_end < i && buf[i] == '"') {
                string_end = get_string_end(i);
                last_was_factor = false;
                continue;
              }
              
              if(buf[i] == PMATH_CHAR_ALIASDELIMITER) {
                in_alias = !in_alias;
                last_was_factor = false;
                continue;
              }
              
              if(i <= string_end || in_alias || e >= self.glyphs.length())
                continue;
                
              const uint16_t *op = buf;
              int ii = i;
              int ee = e;
              if(op[ii] == PMATH_CHAR_BOX)
                find_box_token(op, ii, ee, box);
                
              int prec;
              pmath_token_t tok = pmath_token_analyse(op + ii, ee - ii + 1, &prec);
              float space_left  = 0.0;
              float space_right = 0.0;
              
              bool lwf = false; // new last_was_factor
              bool lwl = false; // new last_was_left
              switch(tok) {
                case PMATH_TOK_PLUSPLUS: {
                    if(self.spans.is_operand_start(i)) {
                      prec = PMATH_PREC_CALL;
                      goto PREFIX;
                    }
                    
                    if( e + 1 < self.glyphs.length() &&
                        self.spans.is_operand_start(e + 1))
                    {
                      goto INFIX;
                    }
                    
                    prec = PMATH_PREC_CALL;
                  }
                  goto POSTFIX;
                  
                case PMATH_TOK_NARY_OR_PREFIX: {
                    if(self.spans.is_operand_start(i)) {
                      prec = pmath_token_prefix_precedence(op + ii, ee - ii + 1, prec);
                      goto PREFIX;
                    }
                  }
                  goto INFIX;
                  
                case PMATH_TOK_NARY_AUTOARG:
                case PMATH_TOK_BINARY_LEFT:
                case PMATH_TOK_BINARY_RIGHT:
                case PMATH_TOK_NARY:
                case PMATH_TOK_QUESTION: {
                  INFIX:
                    switch(prec) {
                      case PMATH_PREC_SEQ:
                      case PMATH_PREC_EVAL:
                        space_right = self.em * 6 / 18;
                        break;
                        
                      case PMATH_PREC_ASS:
                      case PMATH_PREC_MODY:
                        space_left  = self.em * 4 / 18;
                        space_right = self.em * 8 / 18;
                        break;
                        
                      case PMATH_PREC_LAZY:
                      case PMATH_PREC_REPL:
                      case PMATH_PREC_RULE:
                      case PMATH_PREC_MAP:
                      case PMATH_PREC_STR:
                      case PMATH_PREC_COND:
                      case PMATH_PREC_ARROW:
                      case PMATH_PREC_REL:
                        space_left = space_right = self.em * 5 / 18; // total: 10/18 em
                        break;
                        
                      case PMATH_PREC_ALT:
                      case PMATH_PREC_OR:
                      case PMATH_PREC_XOR:
                      case PMATH_PREC_AND:
                      case PMATH_PREC_UNION:
                      case PMATH_PREC_ISECT:
                      case PMATH_PREC_RANGE:
                      case PMATH_PREC_ADD:
                      case PMATH_PREC_PLUMI:
                        space_left = space_right = self.em * 4 / 18; // total: 8/18 em
                        break;
                        
                      case PMATH_PREC_CIRCADD:
                      case PMATH_PREC_CIRCMUL:
                      case PMATH_PREC_MUL:
                      case PMATH_PREC_DIV:
                      case PMATH_PREC_MIDDOT:
                      case PMATH_PREC_MUL2:
                        space_left = space_right = self.em * 3 / 18; // total: 6/18 em
                        break;
                        
                      case PMATH_PREC_CROSS:
                      case PMATH_PREC_POW:
                      case PMATH_PREC_APL:
                      case PMATH_PREC_TEST:
                        space_left = space_right = self.em * 2 / 18; // total: 4/18 em
                        break;
                        
                      case PMATH_PREC_REPEAT:
                      case PMATH_PREC_INC:
                      case PMATH_PREC_CALL:
                      case PMATH_PREC_DIFF:
                      case PMATH_PREC_PRIM:
                        break;
                    }
                  }
                  break;
                  
                case PMATH_TOK_POSTFIX_OR_PREFIX:
                  if(!self.spans.is_operand_start(i))
                    goto POSTFIX;
                    
                  prec = pmath_token_prefix_precedence(op + ii, ee - ii + 1, prec);
                  goto PREFIX;
                  
                case PMATH_TOK_PREFIX: {
                  PREFIX:
                    switch(prec) {
                      case PMATH_PREC_REL: // not
                        space_right = self.em * 4 / 18;
                        break;
                        
                      case PMATH_PREC_ADD:
                        space_right = self.em * 1 / 18;
                        break;
                        
                      case PMATH_PREC_DIV:
                        if(op[ii] == PMATH_CHAR_INTEGRAL_D) {
                          space_left = self.em * 3 / 18;
                        }
                        break;
                        
                      default: break;
                    }
                  }
                  break;
                  
                case PMATH_TOK_POSTFIX: {
                  POSTFIX:
                    switch(prec) {
                      case PMATH_PREC_FAC:
                      case PMATH_PREC_FUNC:
                        space_left = self.em * 2 / 18;
                        break;
                        
                      default: break;
                    }
                  }
                  break;
                  
                case PMATH_TOK_COLON:
                case PMATH_TOK_ASSIGNTAG:
                  space_left = space_right = self.em * 4 / 18;
                  break;
                  
                case PMATH_TOK_NEWLINE:
                  space_left = space_right = 0.0;
                  break;
                  
                case PMATH_TOK_SPACE: {
                    // implicit multiplication:
                    if( buf[i] == ' '           &&
                        e + 1 < self.glyphs.length() &&
                        last_was_factor && context->multiplication_sign)
                    {
                      pmath_token_t tok2 = pmath_token_analyse(buf + e + 1, 1, nullptr);
                      
                      if(buf[e + 1] == PMATH_CHAR_BOX) {
                        Box *next_box = self.boxes[self.get_box(e + 1, box)];
                        
                        tok2 = get_box_start_token(next_box);
                      }
                      
                      if(tok2 == PMATH_TOK_DIGIT || tok2 == PMATH_TOK_LEFTCALL) {
                        context->math_shaper->decode_token(
                          context,
                          1,
                          &context->multiplication_sign,
                          &self.glyphs[i]);
                          
                        space_left = space_right = self.em * 3 / 18;
                        
                        //if(context->show_auto_styles)
                        self.glyphs[i].style = GlyphStyleImplicit;
                      }
                    }
                    else {
                      last_was_space = true;
                      continue;
                    }
                  } break;
                  
                case PMATH_TOK_DIGIT:
                  group_number_digits(i, e);
                /* fall through */
                case PMATH_TOK_STRING:
                case PMATH_TOK_NAME:
                case PMATH_TOK_NAME2:
                  lwf = true;
                /* fall through */
                case PMATH_TOK_SLOT:
                  if(last_was_factor) {
                    space_left = self.em * 3 / 18;
                  }
                  break;
                  
                case PMATH_TOK_LEFT:
                  lwl = true;
                  if(last_was_factor) {
                    space_left = self.em * 3 / 18;
                  }
                  break;
                  
                case PMATH_TOK_RIGHT:
                  if(last_was_left) {
                    space_left = self.em * 3 / 18;
                  }
                  lwf = true;
                  break;
                  
                case PMATH_TOK_PRETEXT:
                  if(i + 1 == e && buf[i] == '<') {
                    self.glyphs[e].x_offset -= self.em * 4 / 18;
                    self.glyphs[e].right -=    self.em * 2 / 18;
                  }
                  break;
                  
                case PMATH_TOK_LEFTCALL:
                  lwl = true;
                  break;
                  
                case PMATH_TOK_NONE:
                case PMATH_TOK_CALL:
                case PMATH_TOK_TILDES:
                case PMATH_TOK_INTEGRAL:
                case PMATH_TOK_COMMENTEND:
                  break;
              }
              
              //last_was_number = tok == PMATH_TOK_DIGIT;
              last_was_factor = lwf;
              last_was_left   = lwl;
              
              if(last_was_space) {
                last_was_space = false;
                space_left     = 0;
              }
              
              if(i > 0 || e + 1 < self.glyphs.length()) {
                if(i > 0) {
                  self.glyphs[i-1].right += space_left / 2;
                  space_left-= space_left / 2;
                }
                self.glyphs[i].x_offset += space_left;
                self.glyphs[i].right +=    space_left;
                if(e + 1 < self.glyphs.length()) {
                  self.glyphs[e + 1].x_offset += space_right / 2;
                  self.glyphs[e + 1].right +=    space_right / 2;
                  space_right-= space_right / 2;
                }
                
                self.glyphs[e].right += space_right;
              }
            }
          }
      };
    public:
    
      void enlarge_space(Context *context) {
        EnlargeSpace(self, context).run();
      }
      
      //}
      
      //{ horizontal stretching (variable width)
    public:
      void hstretch_lines(
        float width,
        float window_width,
        float *unfilled_width
      ) {
        *unfilled_width = -HUGE_VAL;
        
        if(width == HUGE_VAL) {
          if(window_width == HUGE_VAL)
            return;
            
          width = window_width;
        }
        
        const uint16_t *buf = self.str.buffer();
        
        int box = 0;
        int start = 0;
        
        float delta_x = 0;
        for(int line = 0; line < self.lines.length(); line++) {
          float total_fill_weight = 0;
          float white = 0;
          
          int oldbox = box;
          for(int pos = start; pos < self.lines[line].end; ++pos) {
            if(buf[pos] == PMATH_CHAR_BOX) {
              while(self.boxes[box]->index() < pos)
                ++box;
                
              auto fillbox = dynamic_cast<FillBox*>(self.boxes[box]);
              if(fillbox && fillbox->weight > 0) {
                total_fill_weight += fillbox->weight;
                white += fillbox->extents().width;
              }
            }
            
            self.glyphs[pos].right += delta_x;
          }
          
          float line_width = self.indention_width(line);
          if(start > 0)
            line_width -= self.glyphs[start - 1].right;
          if(self.lines[line].end > 0)
            line_width += self.glyphs[self.lines[line].end - 1].right;
            
          if(total_fill_weight > 0) {
            if(width - line_width > 0) {
              float dx = 0;
              
              white += width - line_width;
              
              box = oldbox;
              for(int pos = start; pos < self.lines[line].end; ++pos) {
                if(buf[pos] == PMATH_CHAR_BOX) {
                  while(self.boxes[box]->index() < pos)
                    ++box;
                    
                  auto fillbox = dynamic_cast<FillBox*>(self.boxes[box]);
                  if(fillbox && fillbox->weight > 0) {
                    BoxSize size = fillbox->extents();
                    dx -= size.width;
                    
                    size.width = white * fillbox->weight / total_fill_weight;
                    fillbox->expand(size);
                    
                    dx += size.width;
                    
                    if(self.lines[line].ascent < fillbox->extents().ascent)
                      self.lines[line].ascent = fillbox->extents().ascent;
                      
                    if(self.lines[line].descent < fillbox->extents().descent)
                      self.lines[line].descent = fillbox->extents().descent;
                  }
                }
                
                self.glyphs[pos].right += dx;
              }
              
              delta_x += dx;
            }
          }
          
          line_width += self.indention_width(self.lines[line].indent);
          if(*unfilled_width < line_width)
            *unfilled_width = line_width;
            
          start = self.lines[line].end;
        }
      }
      
      //}
      
      //{ line breaking/indentation
    private:
      /* indention_array[i]: indention of the next line, if there is a line break
         before the i-th character.
       */
      static Array<int> indention_array;
      
      /* penalty_array[i]: A penalty value, which is used to decide whether a line
         break should be placed after (testme: before???) the i-th character.
         The higher this value is, the lower is the probability of a line break after
         character i.
       */
      static Array<double> penalty_array;
      
      static const double DepthPenalty;
      static const double WordPenalty;
      static const double BestLineWidth;
      static const double LineWidthFactor;
      
      static Array<BreakPositionWithPenalty>  break_array;
      static Array<int>                       break_result;
      
    private:
      int fill_block_body_penalty_array(Span span, int depth, int pos, int *box) {
        if(!span)
          return fill_penalty_array(span, depth, pos, box);
          
        int next = fill_penalty_array(span, depth, pos, box);
        
        const uint16_t *buf = self.str.buffer();
        if(buf[next - 1] == ')' && next >= 2) {
          penalty_array[next - 2] = MAX(0, penalty_array[next - 1] - 2 * DepthPenalty);
        }
        
        return next;
      }
      
      int fill_penalty_array(Span span, int depth, int pos, int *box) {
        const uint16_t *buf = self.str.buffer();
        
        if(!span) {
          if(pos > 0) {
            if( buf[pos] == ',' ||
                buf[pos] == ';' ||
                buf[pos] == ':' ||
                buf[pos] == PMATH_CHAR_ASSIGN ||
                buf[pos] == PMATH_CHAR_ASSIGNDELAYED)
            {
              penalty_array[pos - 1] += DepthPenalty;
              //--depth;
            }
            
            if( buf[pos] == 0xA0   /* \[NonBreakingSpace] */ ||
                buf[pos] == 0x2011 /* non breaking hyphen */ ||
                buf[pos] == 0x2060 /* \[NonBreak] */)
            {
              penalty_array[pos - 1] = Infinity;
              penalty_array[pos]   = Infinity;
              ++pos;
            }
          }
          
          if(buf[pos] == PMATH_CHAR_BOX && pos > 0) {
            self.ensure_boxes_valid();
            
            while(self.boxes[*box]->index() < pos)
              ++*box;
              
            if(dynamic_cast<SubsuperscriptBox *>(self.boxes[*box])) {
              penalty_array[pos - 1] = Infinity;
              return pos + 1;
            }
            
            if( self.spans.is_operand_start(pos - 1) &&
                dynamic_cast<GridBox *>(self.boxes[*box]))
            {
              if(pos > 0 && self.spans.is_operand_start(pos - 1)) {
                if(buf[pos - 1] == PMATH_CHAR_PIECEWISE) {
                  penalty_array[pos - 1] = Infinity;
                  return pos + 1;
                }
                
                pmath_token_t tok = pmath_token_analyse(buf + pos - 1, 1, nullptr);
                
                if(tok == PMATH_TOK_LEFT || tok == PMATH_TOK_LEFTCALL)
                  penalty_array[pos - 1] = Infinity;
              }
              
              if(pos + 1 < self.glyphs.length()) {
                pmath_token_t tok = pmath_token_analyse(buf + pos + 1, 1, nullptr);
                
                if(tok == PMATH_TOK_RIGHT)
                  penalty_array[pos] = Infinity;
                return pos + 1;
              }
            }
          }
          
          if(!self.spans.is_operand_start(pos))
            depth++;
            
          if(buf[pos] == ' ') {
            penalty_array[pos] += depth * DepthPenalty;
            
            return pos + 1;
          }
          
          while(pos < self.spans.length() && !self.spans.is_token_end(pos)) {
            penalty_array[pos] += depth * DepthPenalty + WordPenalty;
            ++pos;
          }
          
          if(pos < self.spans.length()) {
            penalty_array[pos] += depth * DepthPenalty;
            ++pos;
          }
          
          return pos;
        }
        
        ++depth;
        
        int (MathSequenceImpl::*fpa)(Span, int, int, int*) = &MathSequenceImpl::fill_penalty_array;
        {
          SpanExpr span_expr(pos, span, &self);
          if(BlockSpan::maybe_block(&span_expr))
            fpa = &MathSequenceImpl::fill_block_body_penalty_array;
        }
        
        int next = (this->*fpa)(span.next(), depth, pos, box);
        
        if(pmath_char_is_left(buf[pos])) {
          penalty_array[pos] += WordPenalty + DepthPenalty;
        }
        
        if(buf[pos] == '"' && !span.next()) {
          ++depth;
          
          penalty_array[pos] = Infinity;
          
          bool last_was_special = false;
          while(next < span.end()) {
            pmath_token_t tok = pmath_token_analyse(buf + next, 1, nullptr);
            penalty_array[next] += depth * DepthPenalty + WordPenalty;
            
            switch(tok) {
              case PMATH_TOK_SPACE:
                penalty_array[next] -= WordPenalty;
                last_was_special = false;
                break;
                
              case PMATH_TOK_STRING:
                last_was_special = false;
                break;
                
              case PMATH_TOK_NAME:
              case PMATH_TOK_NAME2:
              case PMATH_TOK_DIGIT:
                if(last_was_special)
                  penalty_array[next - 1] -= WordPenalty;
                last_was_special = false;
                break;
                
              default:
                last_was_special = true;
                break;
            }
            
            ++next;
          }
          return next;
        }
        
        int func_depth = depth - 1;
        float inc_penalty = 0.0;
        float dec_penalty = 0.0;
        while(next <= span.end()) {
          switch(buf[next]) {
            case ';': dec_penalty = DepthPenalty; break;
            
            case PMATH_CHAR_ASSIGN:
            case PMATH_CHAR_ASSIGNDELAYED:
            case PMATH_CHAR_RULE:
            case PMATH_CHAR_RULEDELAYED:   inc_penalty = DepthPenalty; break;
            
            case ':': {
                if( (next + 2 <= span.end() &&  buf[next + 1] == ':' && buf[next + 2] == '=') ||
                    (next + 1 <= span.end() && (buf[next + 1] == '>' || buf[next + 1] == '=')))
                {
                  inc_penalty = DepthPenalty;
                }
              } break;
              
            case '-': {
                if(next + 1 <= span.end() && (buf[next + 1] == '>' || buf[next + 1] == '='))
                  inc_penalty = DepthPenalty;
              } break;
              
            case '+': {
                if(next + 1 <= span.end() && buf[next + 1] == '=')
                  inc_penalty = DepthPenalty;
              } break;
              
            default:
              if(pmath_char_is_left(buf[next])) {
                if(self.spans.is_operand_start(next))
                  penalty_array[next] += WordPenalty;
                else
                  penalty_array[next - 1] += WordPenalty;
                  
                depth = func_depth;
              }
              else if(pmath_char_is_right(buf[next])) {
                penalty_array[next - 1] += WordPenalty;
              }
          }
          
          next = (this->*fpa)(self.spans[next], depth, next, box);
        }
        
        inc_penalty -= dec_penalty;
        if(inc_penalty != 0) {
          for(; pos < next; ++pos)
            penalty_array[pos] += inc_penalty;
        }
        
        return next;
      }
      
      int fill_string_indentation(Span span, int depth, int pos) {
        const uint16_t *buf = self.str.buffer();
        
        assert(buf[pos] == '\"');
        assert(span);
        assert(!span.next());
        
        int next = fill_indention_array(span.next(), depth + 1, pos);
        indention_array[pos] = depth;
        
        while(next <= span.end()) {
          if(next > 0 && buf[next - 1] == '\n') {
            indention_array[next] = depth;
            ++next;
          }
          else {
            next = fill_indention_array(self.spans[next], depth + 1, next);
          }
        }
        
        return next;
      }
      
      int fill_block_body_indention_array(Span span, int depth, int pos) {
        if(!span)
          return fill_indention_array(span, depth, pos);
          
        int next = fill_indention_array(span, depth, pos);
        
        const uint16_t *buf = self.str.buffer();
        if(buf[pos] == '{' && buf[next - 1] == '}' && !span.next()) {
          /* Unindent closing brace of a block.
               Block {
                 body
               }
             instead of
               Block {
                 body
                 }
           */
          indention_array[next - 1] = MAX(0, depth - 1);
        }
        else if(buf[next - 1] == ')') {
          /* Unindent closing parenthesis of a block header.
               If(
                 cond
               ) {
                 body
               }
             instead of
               If(
                 cond
                 ) {
                 body
               }
           */
          indention_array[next - 1] = MAX(0, depth - 1);
        }
        
        return next;
      }
      
      int fill_indention_array(Span span, int depth, int pos) {
        const uint16_t *buf = self.str.buffer();
        
        if(!span) {
          indention_array[pos] = depth;
          
          ++pos;
          if(pos == self.spans.length() || self.spans.is_token_end(pos - 1))
            return pos;
            
          ++depth;
          do {
            indention_array[pos] = depth;
            ++pos;
          } while(pos < self.spans.length() && !self.spans.is_token_end(pos - 1));
          
          return pos;
        }
        
        if(buf[pos] == '\"' && !span.next())
          return fill_string_indentation(span, depth, pos);
          
        int (MathSequenceImpl::*fia)(Span, int, int) = &MathSequenceImpl::fill_indention_array;
        
        {
          SpanExpr span_expr(pos, span, &self);
          if(BlockSpan::maybe_block(&span_expr))
            fia = &MathSequenceImpl::fill_block_body_indention_array;
        }
        
        int inner_depth = depth + 1;
        if(buf[pos] == '\n' && !span.next())
          inner_depth = depth;
        
        int next = (this->*fia)(span.next(), inner_depth, pos);
        
        bool prev_simple = false;
        bool ends_with_newline = false;
        bool inner_newline = false;
        while(next <= span.end()) {
          Span sub = self.spans[next];
          if((buf[next] == ';' || buf[next] == '\n') && !sub && !prev_simple)
            inner_newline = true;
            
          ends_with_newline = buf[next] == '\n' && !sub;
          prev_simple = !sub;
          next = (this->*fia)(sub, inner_depth, next);
        }
        
        if(ends_with_newline) {
          /* span = {{foo...}, "\n"}.  Treat as {foo...} */
          for(int i = pos; i < next; ++i)
            indention_array[i] = MAX(0, indention_array[i] - 1);
          return next;
        }
        
        indention_array[pos] = depth;
        if(inner_newline) {
          for(int i = pos + 1; i < next; ++i)
            indention_array[i] = MAX(0, indention_array[i] - 1);
        }
        
        return next;
      }
      
      void new_line(int pos, unsigned int indent, bool continuation = false) {
        int len = self.lines.length();
        if( self.lines[len - 1].end < pos ||
            pos == 0 ||
            (len >= 2 && self.lines[len - 2].end >= pos))
        {
          return;
        }
        
        self.lines.length(len + 1);
        self.lines[len].end = self.lines[len - 1].end;
        self.lines[len - 1].end = pos;
        self.lines[len - 1].continuation = continuation;
        
        self.lines[len].ascent = self.lines[len].descent = 0;
        self.lines[len].indent = indent;
        self.lines[len].continuation = 0;
        return;
      }
      
    public:
      void split_lines(Context *context) {
        if(self.glyphs.length() == 0)
          return;
          
        const uint16_t *buf = self.str.buffer();
        
        if(self.glyphs[self.glyphs.length() - 1].right <= context->width) {
          bool have_newline = false;
          
          for(int i = 0; i < self.glyphs.length(); ++i)
            if(buf[i] == '\n') {
              have_newline = true;
              break;
            }
            
          if(!have_newline)
            return;
        }
        
        indention_array.length(self.glyphs.length() + 1);
        penalty_array.length(self.glyphs.length());
        
        indention_array.zeromem();
        penalty_array.zeromem();
        
        int pos = 0;
        while(pos < self.glyphs.length())
          pos = fill_indention_array(self.spans[pos], 0, pos);
          
        indention_array[self.glyphs.length()] = indention_array[self.glyphs.length() - 1];
        
        int box = 0;
        pos = 0;
        while(pos < self.glyphs.length())
          pos = fill_penalty_array(self.spans[pos], 0, pos, &box);
          
        if(buf[self.glyphs.length() - 1] != '\n')
          penalty_array[self.glyphs.length() - 1] = HUGE_VAL;
          
        self._extents.width = context->width;
        for(int start_of_paragraph = 0; start_of_paragraph < self.glyphs.length();) {
          int end_of_paragraph = start_of_paragraph + 1;
          while(end_of_paragraph < self.glyphs.length()
                && buf[end_of_paragraph - 1] != '\n')
            ++end_of_paragraph;
            
          break_array.length(0);
          break_array.add(BreakPositionWithPenalty(start_of_paragraph - 1, 0, 0.0));
          
          for(pos = start_of_paragraph; pos < end_of_paragraph; ++pos) {
            float xend = self.glyphs[pos].right;
            
            int current = break_array.length();
            break_array.add(BreakPositionWithPenalty(pos, -1, Infinity));
            
            for(int i = current - 1; i >= 0; --i) {
              float xstart    = 0;
              float indention = 0;
              double penalty  = break_array[i].penalty;
              if(break_array[i].text_position >= 0) {
                int tp = break_array[i].text_position;
                xstart    = self.glyphs[tp].right;
                indention = self.indention_width(indention_array[tp + 1]);
                penalty  += penalty_array[tp];
              }
              
              if(xend - xstart + indention > context->width
                  && i + 1 < current)
                break;
                
              double best = context->width * BestLineWidth;
              if( pos + 1 < end_of_paragraph ||
                  best < xend - xstart + indention)
              {
                double factor = 0;
                if(context->width > 0)
                  factor = LineWidthFactor / context->width;
                double rel_amplitude = ((xend - xstart + indention) - best) * factor;
                penalty += rel_amplitude * rel_amplitude;
              }
              
              if(!(penalty >= break_array[current].penalty)) {
                break_array[current].penalty = penalty;
                break_array[current].prev_break_index = i;
              }
            }
          }
          
          int mini = break_array.length() - 1;
          for(int i = mini - 1; i >= 0 && break_array[i].text_position + 1 == end_of_paragraph; --i) {
            if(break_array[i].penalty < break_array[mini].penalty)
              mini = i;
          }
          
          if(buf[end_of_paragraph - 1] != '\n')
            mini = break_array[mini].prev_break_index;
            
          break_result.length(0);
          for(int i = mini; i > 0; i = break_array[i].prev_break_index)
            break_result.add(i);
            
          for(int i = break_result.length() - 1; i >= 0; --i) {
            int j = break_result[i];
            pos = break_array[j].text_position;
            
            while(pos + 1 < end_of_paragraph && buf[pos + 1] == ' ')
              ++pos;
              
            if(pos < end_of_paragraph) {
              new_line(
                pos + 1,
                indention_array[pos + 1],
                !self.spans.is_token_end(pos));
            }
          }
          
          start_of_paragraph = end_of_paragraph;
        }
        
        // Move FillBoxes to the beginning of the next line,
        // so aaaaaaaaaaaaaa.........bbbbb will become
        //    aaaaaaaaaaaaaa
        //    ............bbbbb
        // when the window is resized.
        box = 0;
        for(int line = 0; line < self.lines.length() - 1; ++line) {
          if(self.lines[line].end > 0 && buf[self.lines[line].end - 1] == PMATH_CHAR_BOX) {
            while(self.boxes[box]->index() < self.lines[line].end - 1)
              ++box;
              
            if(auto fb = dynamic_cast<FillBox *>(self.boxes[box])) {
              if( buf[self.lines[line].end] == PMATH_CHAR_BOX &&
                  dynamic_cast<FillBox *>(self.boxes[box + 1]))
              {
                continue;
              }
              
              float w = self.glyphs[self.lines[line + 1].end - 1].right - self.glyphs[self.lines[line].end - 1].right;
              
              if(fb->extents().width + w + self.indention_width(self.lines[line + 1].indent) <= context->width) {
                self.lines[line].end--;
              }
            }
          }
        }
      }
      
      //}
      
  };
  
  Array<int>    MathSequenceImpl::indention_array(0);
  Array<double> MathSequenceImpl::penalty_array(0);
  
  const double MathSequenceImpl::DepthPenalty = 1.0;
  const double MathSequenceImpl::WordPenalty = 100.0;//2.0;
  const double MathSequenceImpl::BestLineWidth = 0.95;
  const double MathSequenceImpl::LineWidthFactor = 2.0;
  
  Array<BreakPositionWithPenalty>  MathSequenceImpl::break_array(0);
  Array<int>                       MathSequenceImpl::break_result(0);
  
}

//{ class MathSequence ...

MathSequence::MathSequence()
  : AbstractSequence(),
    str(""),
    boxes_invalid(false),
    spans_invalid(false)
{
}

MathSequence::~MathSequence() {
  for(int i = 0; i < boxes.length(); ++i)
    delete boxes[i];
}

Box *MathSequence::item(int i) {
  ensure_boxes_valid();
  return boxes[i];
}

String MathSequence::raw_substring(int start, int length) {
  assert(start >= 0);
  assert(length >= 0);
  assert(start + length <= str.length());
  
  return str.part(start, length);
}

uint32_t MathSequence::char_at(int pos) {
  if(pos < 0 || pos > str.length())
    return 0;
    
  const uint16_t *buf = str.buffer();
  
  if(is_utf16_high(buf[pos]) && is_utf16_low((buf[pos + 1]))) {
    uint32_t hi = buf[pos];
    uint32_t lo = buf[pos + 1];
    
    return 0x10000 + (((hi & 0x03FF) << 10) | (lo & 0x03FF));
  }
  
  return buf[pos];
}

bool MathSequence::expand(const BoxSize &size) {
  if(boxes.length() == 1 && glyphs.length() == 1 && str.length() == 1) {
    if(boxes[0]->expand(size)) {
      _extents = boxes[0]->extents();
      glyphs[0].right  = _extents.width;
      lines[0].ascent  = _extents.ascent;
      lines[0].descent = _extents.descent;
      return true;
    }
  }
  else {
    float uw;
    float w = _extents.width;
    
    MathSequenceImpl(*this).hstretch_lines(
      size.width,
      size.width,
      &uw);
      
    return w != _extents.width;
  }
  
  return false;
}

void MathSequence::resize(Context *context) {
  glyphs.length(str.length());
  glyphs.zeromem();
  
  ensure_boxes_valid();
  ensure_spans_valid();
  
  em = context->canvas->get_font_size();
  
  float old_scww = context->section_content_window_width;
  context->section_content_window_width = HUGE_VAL;
  
  int box = 0;
  int pos = 0;
  while(pos < glyphs.length())
    MathSequenceImpl(*this).resize_span(context, spans[pos], &pos, &box);
    
  MathSequenceImpl(*this).apply_glyph_substitutions(context);
  
  if(context->show_auto_styles) {
    ScopeColorizer colorizer(this);
    
    pos = 0;
    while(pos < glyphs.length())
      colorizer.comments_colorize_span(spans[pos], &pos);
      
    pos = 0;
    while(pos < glyphs.length()) {
      SpanExpr *se = new SpanExpr(pos, spans[pos], this);
      
      if(se->count() == 0 || !se->item_as_text(0).equals("/*")) {
        colorizer.syntax_colorize_spanexpr(        se);
        colorizer.arglist_errors_colorize_spanexpr(se, em * RefErrorIndictorHeight);
      }
      
      pos = se->end() + 1;
      delete se;
    }
  }
  
  if(context->math_spacing) {
    float ca = 0;
    float cd = 0;
    float a = 0;
    float d = 0;
    
    if(glyphs.length() == 1 &&
        !dynamic_cast<UnderoverscriptBox *>(_parent))
    {
      pmath_token_t tok = pmath_token_analyse(str.buffer(), 1, nullptr);
      
      if(tok == PMATH_TOK_INTEGRAL || tok == PMATH_TOK_PREFIX) {
        context->math_shaper->vertical_stretch_char(
          context,
          a,
          d,
          true,
          str[0],
          &glyphs[0]);
          
        BoxSize size;
        context->math_shaper->vertical_glyph_size(
          context,
          str[0],
          glyphs[0],
          &size.ascent,
          &size.descent);
          
        size.bigger_y(&ca, &cd);
        size.bigger_y(&a,  &d);
      }
      else {
        box = 0;
        pos = 0;
        while(pos < glyphs.length())
          MathSequenceImpl(*this).stretch_span(context, spans[pos], &pos, &box, &ca, &cd, &a, &d);
      }
    }
    else {
      box = 0;
      pos = 0;
      while(pos < glyphs.length())
        MathSequenceImpl(*this).stretch_span(context, spans[pos], &pos, &box, &ca, &cd, &a, &d);
    }
    
    MathSequenceImpl(*this).enlarge_space(context);
  }
  
  {
    _extents.width = 0;
    const uint16_t *buf = str.buffer();
    for(pos = 0; pos < glyphs.length(); ++pos)
      if(buf[pos] == '\n')
        glyphs[pos].right = _extents.width;
      else
        glyphs[pos].right = _extents.width += glyphs[pos].right;
  }
  
  lines.length(1);
  lines[0].end = glyphs.length();
  lines[0].ascent = lines[0].descent = 0;
  lines[0].indent = 0;
  lines[0].continuation = 0;
  
  context->section_content_window_width = old_scww;
  
  MathSequenceImpl(*this).split_lines(context);
  if(dynamic_cast<Section *>(_parent)) {
    MathSequenceImpl(*this).hstretch_lines(
      context->width,
      context->section_content_window_width,
      &context->sequence_unfilled_width);
  }
  
  const uint16_t *buf = str.buffer();
  int line = 0;
  pos = 0;
  box = 0;
  float x = 0;
  _extents.descent = _extents.width = 0;
  if(lines.length() > 1) {
    lines[0].ascent  = 0.75f * em;
    lines[0].descent = 0.25f * em;
  }
  while(pos < glyphs.length()) {
    if(pos == lines[line].end) {
      if(pos > 0) {
        double indent = indention_width(lines[line].indent);
        
        if(_extents.width < glyphs[pos - 1].right - x + indent)
          _extents.width  = glyphs[pos - 1].right - x + indent;
        x = glyphs[pos - 1].right;
      }
      
      _extents.descent += lines[line].ascent + lines[line].descent + line_spacing();
      
      ++line;
      lines[line].ascent  = 0.75f * em;
      lines[line].descent = 0.25f * em;
    }
    
    if(buf[pos] == PMATH_CHAR_BOX) {
      boxes[box]->extents().bigger_y(&lines[line].ascent, &lines[line].descent);
      ++box;
    }
    else if(glyphs[pos].is_normal_text) {
      context->text_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        &lines[line].ascent,
        &lines[line].descent);
    }
    else {
      context->math_shaper->vertical_glyph_size(
        context,
        buf[pos],
        glyphs[pos],
        &lines[line].ascent,
        &lines[line].descent);
    }
    
    ++pos;
  }
  
  if(pos > 0) {
    double indent = indention_width(lines[line].indent);
    
    if(_extents.width < glyphs[pos - 1].right - x + indent)
      _extents.width  = glyphs[pos - 1].right - x + indent;
  }
  
  if(line + 1 < lines.length()) {
    _extents.descent += lines[line].ascent + lines[line].descent;
    ++line;
    lines[line].ascent = 0.75f * em;
    lines[line].descent = 0.25f * em;
  }
  _extents.ascent = lines[0].ascent;
  _extents.descent += lines[line].ascent + lines[line].descent - lines[0].ascent;
  
  if(_extents.width < 0.75 && lines.length() > 1) {
    _extents.width = 0.75;
  }
  
//  // round ascent/descent to next multiple of 0.75pt (= 1px by default):
//  _extents.ascent  = ceilf(_extents.ascent  / 0.75f ) * 0.75f;
//  _extents.descent = ceilf(_extents.descent / 0.75f ) * 0.75f;

  if(context->sequence_unfilled_width == -HUGE_VAL)
    context->sequence_unfilled_width = _extents.width;
}

void MathSequence::colorize_scope(SyntaxState *state) {
  assert(glyphs.length() == spans.length());
  assert(glyphs.length() == str.length());
  
  ScopeColorizer colorizer(this);
  
  int pos = 0;
  while(pos < glyphs.length()) {
    SpanExpr *se = new SpanExpr(pos, spans[pos], this);
    
    colorizer.scope_colorize_spanexpr(state, se);
    
    pos = se->end() + 1;
    delete se;
  }
}

void MathSequence::paint(Context *context) {
  float x0, y0;
  context->canvas->current_pos(&x0, &y0);
  
  Color default_color = context->canvas->get_color();
  SharedPtr<MathShaper> default_math_shaper = context->math_shaper;
  
  {
    context->syntax->glyph_style_colors[GlyphStyleNone] = default_color;
    AutoCallPaintHooks auto_hooks(this, context);
    
    float y = y0;
    if(lines.length() > 0)
      y -= lines[0].ascent;
      
    const uint16_t *buf = str.buffer();
    
    double clip_x1, clip_y1, clip_x2, clip_y2;
    context->canvas->clip_extents(&clip_x1, &clip_y1, &clip_x2,  &clip_y2);
    
    int line = 0;
    // skip invisible lines:
    while(line < lines.length()) {
      float h = lines[line].ascent + lines[line].descent;
      if(y + h >= clip_y1)
        break;
        
      y += h + line_spacing();
      ++line;
    }
    
    if(line < lines.length()) {
      float glyph_left = 0;
      int box = 0;
      int pos = 0;
      
      if(line > 0)
        pos = lines[line - 1].end;
        
      if(pos > 0)
        glyph_left = glyphs[pos - 1].right;
        
      bool have_style = false;
      bool have_slant = false;
      for(; line < lines.length() && y < clip_y2; ++line) {
        float x_extra = x0 + indention_width(lines[line].indent);
        
//#ifndef NDEBUG
//        {
//          int old_color = context->canvas->get_color();
//          context->canvas->save();
//          context->canvas->set_color(0x808080);
//          
//          for(int i = 0; i < lines[line].indent; ++i) {
//            context->canvas->move_to(
//              x0 + i * (x_extra - x0) / lines[line].indent,
//              y + lines[line].ascent);
//            context->canvas->rel_line_to(0, -0.75);
//          }
//          context->canvas->stroke();
//          
//          context->canvas->set_color(old_color);
//          context->canvas->restore();
//        }
//#endif
        
        if(pos > 0)
          x_extra -= glyphs[pos - 1].right;
        
//        if(pos < glyphs.length()) {
//          if(pos > 0 && buf[pos-1] != '\n')
//            x_extra -= glyphs[pos].x_offset;
//        }
        y += lines[line].ascent;
        
        for(; pos < lines[line].end; ++pos) {
          if(buf[pos] == '\n') {
            glyph_left = glyphs[pos].right;
            continue;
          }
          
          if(have_style || glyphs[pos].style) {
            Color color = context->syntax->glyph_style_colors[glyphs[pos].style];
            
            context->canvas->set_color(color);
            have_style = color != default_color;
          }
          
          if(have_slant || glyphs[pos].slant) {
            if(glyphs[pos].slant == FontSlantItalic) {
              context->math_shaper = default_math_shaper->set_style(
                                       default_math_shaper->get_style() + Italic);
              have_slant = true;
            }
            else if(glyphs[pos].slant == FontSlantPlain) {
              context->math_shaper = default_math_shaper->set_style(
                                       default_math_shaper->get_style() - Italic);
              have_slant = true;
            }
            else {
              context->math_shaper = default_math_shaper;
              have_slant = false;
            }
          }
          
//          #ifndef NDEBUG
//          if(spans.is_operand_start(pos)){
//            context->canvas->save();
//
//            context->canvas->move_to(glyph_left + x_extra + glyphs[pos].x_offset, y - 1.5);
//            context->canvas->rel_line_to(0, 3);
//            context->canvas->rel_line_to(3, 0);
//
//            context->canvas->set_color(0x008000);
//            context->canvas->hair_stroke();
//
//            context->canvas->set_color(default_color);
//            context->canvas->restore();
//          }
//          #endif


          if(buf[pos] == PMATH_CHAR_BOX) {
            while(boxes[box]->index() < pos)
              ++box;
              
            context->canvas->move_to(glyph_left + x_extra + glyphs[pos].x_offset, y);
            
            boxes[box]->paint(context);
            
            context->syntax->glyph_style_colors[GlyphStyleNone] = default_color;
            ++box;
          }
          else if(glyphs[pos].index ||
                  glyphs[pos].composed ||
                  glyphs[pos].horizontal_stretch)
          {
            if(glyphs[pos].is_normal_text) {
              context->text_shaper->show_glyph(
                context,
                glyph_left + x_extra,
                y,
                buf[pos],
                glyphs[pos]);
            }
            else {
              context->math_shaper->show_glyph(
                context,
                glyph_left + x_extra,
                y,
                buf[pos],
                glyphs[pos]);
            }
          }
          
          if(glyphs[pos].missing_after) {
            float d = em * RefErrorIndictorHeight * 2 / 3.0f;
            float dd = d / 4;
            
            context->canvas->move_to(glyphs[pos].right + x_extra, y + em / 8);
            if(pos + 1 < glyphs.length())
              context->canvas->rel_move_to(glyphs[pos + 1].x_offset / 2, 0);
              
            context->canvas->rel_line_to(-d, d);
            context->canvas->rel_line_to(dd, dd);
            context->canvas->rel_line_to(d - dd, dd - d);
            context->canvas->rel_line_to(d - dd, d - dd);
            context->canvas->rel_line_to(dd, -dd);
            context->canvas->rel_line_to(-d, -d);
            
            context->canvas->close_path();
            context->canvas->set_color(
              context->syntax->glyph_style_colors[GlyphStyleExcessOrMissingArg]);
            context->canvas->fill();
            
            have_style = true;
          }
          
          glyph_left = glyphs[pos].right;
        }
        
        if(lines[line].continuation) {
          GlyphInfo gi;
          memset(&gi, 0, sizeof(GlyphInfo));
          uint16_t cont = CHAR_LINE_CONTINUATION;
          context->math_shaper->decode_token(
            context,
            1,
            &cont,
            &gi);
            
          context->math_shaper->show_glyph(
            context,
            glyph_left + x_extra,
            y,
            cont,
            gi);
        }
        
        y += lines[line].descent + line_spacing();
      }
      
    }
    
    if(context->selection.get() == this && !context->canvas->show_only_text) {
      context->canvas->move_to(x0, y0);
      
      selection_path(
        context,
        context->canvas,
        context->selection.start,
        context->selection.end);
        
      context->draw_selection_path();
    }
  }
  
  context->canvas->set_color(default_color);
  context->math_shaper = default_math_shaper;
}

void MathSequence::selection_path(Canvas *canvas, int start, int end) {
  selection_path(0, canvas, start, end);
}

void MathSequence::selection_path(Context *opt_context, Canvas *canvas, int start, int end) {
  float x0, y0, x1, y1, x2, y2;
//  const uint16_t *buf = str.buffer();

  if(start > glyphs.length())
    start = glyphs.length();
  if(end > glyphs.length())
    end = glyphs.length();
    
  canvas->current_pos(&x0, &y0);
  
  y0 -= lines[0].ascent;
  y1 = y0;
  
  int startline = 0;
  while(startline < lines.length() && start >= lines[startline].end) {
    y1 += lines[startline].ascent + lines[startline].descent + line_spacing();
    ++startline;
  }
  
  if(startline == lines.length()) {
    --startline;
    y1 -= lines[startline].ascent + lines[startline].descent + line_spacing();
  }
  
  y2 = y1;
  int endline = startline;
  while(endline < lines.length() && end > lines[endline].end) {
    y2 += lines[endline].ascent + lines[endline].descent + line_spacing();
    ++endline;
  }
  
  if(endline == lines.length()) {
    --endline;
    y2 -= lines[endline].ascent + lines[endline].descent + line_spacing();
  }
  
  x1 = x0;
  if(start > 0)
    x1 += glyphs[start - 1].right;
    
  if(startline > 0)
    x1 -= glyphs[lines[startline - 1].end - 1].right;
    
  x1 += indention_width(lines[startline].indent);
  
  x2 = x0;
  if(end > 0)
    x2 += glyphs[end - 1].right;
    
  if(endline > 0)
    x2 -= glyphs[lines[endline - 1].end - 1].right;
    
  x2 += indention_width(lines[endline].indent);
  
  if(endline == startline) {
    float a = 0.5 * em;
    float d = 0;
    
    if(opt_context) {
      if(start == end) {
        const uint16_t *buf = str.buffer();
        int box = 0;
        
        for(int i = 0; i < start; ++i)
          if(buf[i] == PMATH_CHAR_BOX)
            ++box;
            
        MathSequenceImpl(*this).caret_size(opt_context, start, box, &a, &d);
      }
      else {
        MathSequenceImpl(*this).boxes_size(
          opt_context,
          start,
          end,
          &a, &d);
      }
    }
    else {
      a = lines[startline].ascent;
      d = lines[startline].descent;
    }
    
    y1 += lines[startline].ascent;
    y2 = y1 + d + 1;
    y1 -= a + 1;
    
    if(start == end) {
      canvas->align_point(&x1, &y1, true);
      canvas->align_point(&x2, &y2, true);
      
      canvas->move_to(x1, y1);
      canvas->line_to(x2, y2);
    }
    else
      canvas->pixrect(x1, y1, x2, y2, false);
  }
  else {
    y2 = y1;
    for(int line = startline; line <= endline; ++line)
      y2 += lines[line].ascent + lines[line].descent + line_spacing();
    y2 -= line_spacing();
    
    /*    1----3
          |    |
      7---8    |
      |      5-4
      |      |
      6------2
     */
    
    float x3, y3, x4, y4, x5, y5, x6, y6, x7, y7, x8, y8;
    
    x3 = x4 = x0 + _extents.width;
    x5 = x2;
    x6 = x7 = x0;
    x8 = x1;
    
    y3 = y1;
    y4 = y5 = y2 - lines[endline].ascent - lines[endline].descent - line_spacing() / 2;
    y6 = y2;
    y7 = y8 = y1 + lines[startline].ascent + lines[startline].descent + line_spacing() / 2;
    
    canvas->align_point(&x1, &y1, false);
    canvas->align_point(&x2, &y2, false);
    canvas->align_point(&x3, &y3, false);
    canvas->align_point(&x4, &y4, false);
    canvas->align_point(&x5, &y5, false);
    canvas->align_point(&x6, &y6, false);
    canvas->align_point(&x7, &y7, false);
    canvas->align_point(&x8, &y8, false);
    
    canvas->move_to(x1, y1);
    canvas->line_to(x3, y3);
    canvas->line_to(x4, y4);
    canvas->line_to(x5, y5);
    canvas->line_to(x2, y2);
    canvas->line_to(x6, y6);
    canvas->line_to(x7, y7);
    canvas->line_to(x8, y8);
    canvas->close_path();
  }
}

Expr MathSequence::to_pmath(BoxOutputFlags flags) {
  return to_pmath(flags, 0, length());
}

Expr MathSequence::to_pmath(BoxOutputFlags flags, int start, int end) {
  ScanData data;
  data.sequence    = this;
  data.current_box = 0;
  data.flags       = flags;
  data.start       = start;
  data.end         = end;
  
  struct pmath_boxes_from_spans_ex_t settings;
  memset(&settings, 0, sizeof(settings));
  settings.size           = sizeof(settings);
  settings.data           = &data;
  settings.box_at_index   = MathSequenceImpl::box_at_index;
  settings.add_debug_info = MathSequenceImpl::add_debug_info;
  
  if(has(flags, BoxOutputFlags::Parseable))
    settings.flags |= PMATH_BFS_PARSEABLE;
    
  settings.flags |= PMATH_BFS_USECOMPLEXSTRINGBOX;
  
  ensure_spans_valid();
  
  pmath_t boxes = pmath_boxes_from_spans_ex(spans.array(), str.get(), &settings);
  if(start > 0 || end < length())
    boxes = MathSequenceImpl::remove_null_tokens(boxes);
  return Expr(boxes);
//  if(start == 0 && end >= length())
//    return to_pmath(flags);
//
//  const uint16_t *buf = str.buffer();
//  int firstbox = 0;
//
//  for(int i = 0; i < start; ++i)
//    if(buf[i] == PMATH_CHAR_BOX)
//      ++firstbox;
//
//  MathSequence *tmp = new MathSequence();
//  tmp->insert(0, this, start, end);
//  tmp->ensure_spans_valid();
//  tmp->ensure_boxes_valid();
//
//  Expr result = tmp->to_pmath(flags);
//
//  for(int i = 0; i < tmp->boxes.length(); ++i) {
//    Box *box          = boxes[firstbox + i];
//    Box *tmp_box      = tmp->boxes[i];
//    int box_index     = box->index();
//    int tmp_box_index = tmp_box->index();
//
//    abandon(box);
//    tmp->abandon(tmp_box);
//
//    adopt(tmp_box, box_index);
//    tmp->adopt(box, tmp_box_index);
//
//    boxes[firstbox + i] = tmp_box;
//    tmp->boxes[i] = box;
//  }
//
//  delete tmp;
//  return result;
}

Box *MathSequence::move_logical(
  LogicalDirection  direction,
  bool              jumping,
  int              *index
) {
  const int len = length();
  const uint16_t *buf = str.buffer();
  
  if(direction == LogicalDirection::Forward) {
    if(*index >= len) {
      if(_parent) {
        if(jumping && !_parent->exitable())
          return this;
          
        *index = _index;
        return _parent->move_logical(LogicalDirection::Forward, true, index);
      }
      return this;
    }
    
    if(jumping || *index < 0 || buf[*index] != PMATH_CHAR_BOX) {
      if(jumping) {
        while(*index + 1 < len && !spans.is_token_end(*index))
          ++*index;
          
        ++*index;
        while(*index < len && (buf[*index] == ' ' || buf[*index] == '\t'))
          ++*index;
      }
      else {
        if(*index + 2 < len && is_utf16_high(buf[*index]) && is_utf16_low(buf[*index + 1]))
          ++*index;
          
        ++*index;
      }
      
      return this;
    }
    
    ensure_boxes_valid();
    
    int b = 0;
    while(boxes[b]->index() != *index)
      ++b;
    *index = -1;
    return boxes[b]->move_logical(LogicalDirection::Forward, true, index);
  }
  
  if(*index <= 0) {
    if(_parent) {
      if(jumping && !_parent->exitable())
        return this;
        
      *index = _index + 1;
      return _parent->move_logical(LogicalDirection::Backward, true, index);
    }
    return this;
  }
  
  if(jumping) {
    do {
      --*index;
    } while(*index > 0 && (buf[*index] == ' ' || buf[*index] == '\t'));
    ++*index;
    
    do {
      --*index;
    } while(*index > 0 && !spans.is_token_end(*index - 1));
    
    return this;
  }
  
  if(buf[*index - 1] != PMATH_CHAR_BOX) {
    --*index;
    
    if(*index > 0 && is_utf16_high(buf[*index - 1]) && is_utf16_low(buf[*index]))
      --*index;
      
    return this;
  }
  
  ensure_boxes_valid();
  
  int b = 0;
  while(boxes[b]->index() != *index - 1)
    ++b;
  *index = boxes[b]->length() + 1;
  return boxes[b]->move_logical(LogicalDirection::Backward, true, index);
}

Box *MathSequence::move_vertical(
  LogicalDirection  direction,
  float            *index_rel_x,
  int              *index,
  bool              called_from_child
) {
  int line, dstline;
  float x = *index_rel_x;
  
  if(*index >= 0) {
    line = 0;
    while(line < lines.length() - 1
          && lines[line].end <= *index)
      ++line;
      
    if(*index > 0) {
      x += glyphs[*index - 1].right + indention_width(lines[line].indent);
      if(line > 0)
        x -= glyphs[lines[line - 1].end - 1].right;
    }
    dstline = direction == LogicalDirection::Forward ? line + 1 : line - 1;
  }
  else if(direction == LogicalDirection::Forward) {
    line = -1;
    dstline = 0;
  }
  else {
    line = lines.length();
    dstline = line - 1;
  }
  
  if(dstline >= 0 && dstline < lines.length()) {
    int i = 0;
    float l = indention_width(lines[dstline].indent);
    if(dstline > 0) {
      i = lines[dstline - 1].end;
      l -= glyphs[lines[dstline - 1].end - 1].right;
    }
    
    while(i < lines[dstline].end
          && glyphs[i].right + l < x)
      ++i;
      
    if(i < lines[dstline].end
        && str[i] != PMATH_CHAR_BOX) {
      if( (i == 0 && l +  glyphs[i].right                      / 2 <= x) ||
          (i >  0 && l + (glyphs[i].right + glyphs[i - 1].right) / 2 <= x))
        ++i;
        
      if(is_utf16_high(str[i - 1]))
        --i;
      else if(is_utf16_low(str[i]))
        ++i;
    }
    
    if(i > 0
        && i < glyphs.length()
        && i == lines[dstline].end
        && (direction == LogicalDirection::Backward || str[i - 1] == '\n'))
      --i;
      
    *index_rel_x = x - indention_width(lines[dstline].indent);
    if(i == lines[dstline].end && dstline < lines.length() - 1) {
      *index_rel_x += indention_width(lines[dstline + 1].indent);
    }
    else if(i > 0) {
      *index_rel_x -= glyphs[i - 1].right;
      if(dstline > 0)
        *index_rel_x += glyphs[lines[dstline - 1].end - 1].right;
    }
    
    if(i < glyphs.length()
        && str[i] == PMATH_CHAR_BOX
        && *index_rel_x > 0
        && x < glyphs[i].right + l) {
      ensure_boxes_valid();
      int b = 0;
      while(boxes[b]->index() < i)
        ++b;
      *index = -1;
      return boxes[b]->move_vertical(direction, index_rel_x, index, false);
    }
    
    *index = i;
    return this;
  }
  
  if(_parent) {
    *index_rel_x = x;
    *index = _index;
    return _parent->move_vertical(direction, index_rel_x, index, true);
  }
  
  return this;
}

Box *MathSequence::mouse_selection(
  float  x,
  float  y,
  int   *start,
  int   *end,
  bool   *was_inside_start
) {
  *was_inside_start = true;
  
  if(lines.length() == 0) {
    *start = *end = 0;
    return this;
  }
  
  int line = 0;
  while(line < lines.length() - 1 && y > lines[line].descent + 0.1 * em) {
    y -= lines[line].descent + line_spacing() + lines[line + 1].ascent;
    ++line;
  }
  
  if(line > 0)
    *start = lines[line - 1].end;
  else
    *start = 0;
    
  const uint16_t *buf = str.buffer();
  
  x -= indention_width(lines[line].indent);
//  if(line > 0 && lines[line - 1].end < glyphs.length())
//    x += glyphs[lines[line - 1].end].x_offset;

  if(x < 0) {
    *was_inside_start = false;
    *end = *start;
//    if(is_placeholder(*start))
//      ++*end;
    return this;
  }
  
  float line_start = 0;
  if(*start > 0)
    line_start += glyphs[*start - 1].right;
    
  while(*start < lines[line].end) {
    if(x <= glyphs[*start].right - line_start) {
      float prev = 0;
      if(*start > 0)
        prev = glyphs[*start - 1].right;
        
      if(is_placeholder(*start)) {
        *was_inside_start = true;
        *end = *start + 1;
        return this;
      }
      
      if(buf[*start] == PMATH_CHAR_BOX) {
        ensure_boxes_valid();
        int b = 0;
        while(b < boxes.length() && boxes[b]->index() < *start)
          ++b;
          
        float xoff = glyphs[*start].x_offset;
        if(x > prev - line_start + xoff + boxes[b]->extents().width) {
          *was_inside_start = false;
          ++*start;
          *end = *start;
          return this;
        }
        
        if(x < prev - line_start + xoff) {
          *was_inside_start = false;
          *end = *start;
          return this;
        }
        
        return boxes[b]->mouse_selection(
                 x - (prev - line_start + xoff),
                 y,
                 start,
                 end,
                 was_inside_start);
      }
      
      if(line_start + x > (prev + glyphs[*start].right) / 2) {
        *was_inside_start = false;
        ++*start;
        *end = *start;
        return this;
      }
      
      *end = *start;
      return this;
    }
    
    ++*start;
  }
  
  if(*start > 0) {
    if(buf[*start - 1] == '\n' && (line == 0 || lines[line - 1].end != lines[line].end)) {
      --*start;
    }
    else if(buf[*start - 1] == ' ' && *start < glyphs.length())
      --*start;
  }
  
  *end = *start;
//  if(is_placeholder(*start - 1)){
//    --*start;
//    *was_inside_start = false;
//  }
  return this;
}

void MathSequence::child_transformation(
  int             index,
  cairo_matrix_t *matrix
) {
  if(lines.length() == 0 || index > glyphs.length())
    return;
    
  float x = 0;
  float y = 0;
  
  int l = 0;
  while(l + 1 < lines.length() && lines[l].end <= index) {
    y += lines[l].descent + line_spacing() + lines[l + 1].ascent;
    ++l;
  }
  
  x += indention_width(lines[l].indent);
  
  if(index < glyphs.length()) {
    if(str[index] == PMATH_CHAR_BOX)
      x += glyphs[index].x_offset;
  }
  
  if(index > 0) {
    x += glyphs[index - 1].right;
    
    if(l > 0 && lines[l - 1].end > 0) {
      x -= glyphs[lines[l - 1].end - 1].right;
      
//      if(lines[l - 1].end < glyphs.length())
//        x -= glyphs[lines[l - 1].end].x_offset;
    }
  }
  
  cairo_matrix_translate(matrix, x, y);
}

Box *MathSequence::normalize_selection(int *start, int *end) {
  if(is_utf16_high(str[*start - 1]))
    --*start;
    
  if(is_utf16_low(str[*end]))
    ++*end;
    
  return this;
}

int MathSequence::find_string_start(int pos_inside_string, int *next_afer_string) {
  ensure_spans_valid();
  
  if(next_afer_string)
    *next_afer_string = -1;
    
  const uint16_t *buf = str.buffer();
  int i = 0;
  while(i < pos_inside_string) {
    if(buf[i] == '"') {
      int start = i;
      Span span = spans[i];
      
      while(span.next()) {
        span = span.next();
      }
      
      if(span) {
        i = span.end() + 1;
        if(i > pos_inside_string || (i == pos_inside_string && buf[i - 1] != '"')) {
          if(next_afer_string)
            *next_afer_string = i;
          return start;
        }
      }
      else
        ++i;
    }
    else
      ++i;
  }
  
  return -1;
}

void MathSequence::ensure_boxes_valid() {
  if(!boxes_invalid)
    return;
    
  boxes_invalid = false;
  const uint16_t *buf = str.buffer();
  int len = str.length();
  int box = 0;
  for(int i = 0; i < len; ++i)
    if(buf[i] == PMATH_CHAR_BOX)
      adopt(boxes[box++], i);
}

void MathSequence::ensure_spans_valid() {
  if(!spans_invalid)
    return;
    
  spans_invalid = false;
  
  ScanData data;
  data.sequence    = this;
  data.current_box = 0;
  data.flags       = BoxOutputFlags::Default;
  data.start       = 0;
  data.end         = str.length();
  
  pmath_string_t code = str.get_as_string();
  spans = pmath_spans_from_string(
            &code,
            0,
            MathSequenceImpl::subsuperscriptbox_at_index,
            MathSequenceImpl::underoverscriptbox_at_index,
            MathSequenceImpl::syntax_error,
            &data);
}

bool MathSequence::is_placeholder() {
  return str.length() == 1 && is_placeholder(0);
}

bool MathSequence::is_placeholder(int i) {
  if(i < 0 || i >= str.length())
    return false;
    
  if(str[i] == PMATH_CHAR_PLACEHOLDER || str[i] == PMATH_CHAR_SELECTIONPLACEHOLDER)
    return true;
    
  if(str[i] == PMATH_CHAR_BOX) {
    ensure_boxes_valid();
    int b = 0;
    
    while(boxes[b]->index() < i)
      ++b;
      
    return boxes[b]->get_own_style(Placeholder);
  }
  
  return false;
}

int MathSequence::matching_fence(int pos) {
  int len = str.length();
  if(pos < 0 || pos >= len)
    return -1;
    
  const uint16_t *buf = str.buffer();
  if(pmath_char_is_left(buf[pos]) || pmath_char_is_right(buf[pos])) {
    SpanExpr *span = new SpanExpr(pos, this);
    
    while(span) {
      if(span->start() <= pos && span->end() >= pos && span->length() > 1)
        break;
        
      span = span->expand(true);
    }
    
    if(span) {
      for(int i = 0; i < span->count(); ++i) {
        int tok_pos = span->item_pos(i);
        if(tok_pos != pos) {
          pmath_token_t tok = span->item(i)->as_token();
          
          if(tok == PMATH_TOK_LEFT ||
              tok == PMATH_TOK_LEFTCALL ||
              tok == PMATH_TOK_RIGHT)
          {
            delete span;
            return tok_pos;
          }
        }
      }
      
      delete span;
    }
  }
  
  return -1;
}

//{ insert/remove ...

int MathSequence::insert(int pos, uint16_t chr) {
  if(chr == PMATH_CHAR_BOX) 
    return insert(pos, new ErrorBox(String::FromChar(chr)));
  
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, &chr, 1);
  invalidate();
  return pos + 1;
}

int MathSequence::insert(int pos, const uint16_t *ucs2, int len) {
  if(len < 0) {
    len = 0;
    const uint16_t *buf = ucs2;
    while(*buf++)
      ++len;
  }
  
  int boxpos = 0;
  while(boxpos < len && ucs2[boxpos] != PMATH_CHAR_BOX)
    ++boxpos;
  
  if(boxpos < len) {
    ensure_boxes_valid();
    
    int b = 0;
    while(b < boxes.length() && boxes[b]->index() < pos)
      ++b;
    
    while(boxpos < len) {
      ++boxpos;
      str.insert(pos, ucs2, boxpos);
      
      pos+= boxpos;
      ucs2+= boxpos;
      len-= boxpos;
      
      Box *box = new ErrorBox(String::FromChar(PMATH_CHAR_BOX));
      adopt(box, pos - 1);
      boxes.insert(b, 1, &box);
      ++b;
      
      boxpos = 0;
      while(boxpos < len && ucs2[boxpos] != PMATH_CHAR_BOX)
        ++boxpos;
    }
  }
  str.insert(pos, ucs2, len);
  
  spans_invalid = true;
  boxes_invalid = true;
  invalidate();
  return pos + len;
}

int MathSequence::insert(int pos, const char *latin1, int len) {
  if(len < 0)
    len = strlen(latin1);
  
  spans_invalid = true;
  boxes_invalid = true;
  str.insert(pos, latin1, len);
  invalidate();
  return pos + len;
}

int MathSequence::insert(int pos, const String &s) {
  return insert(pos, s.buffer(), s.length());
}

int MathSequence::insert(int pos, Box *box) {
  if(pos > length())
    pos = length();
    
  if(MathSequence *sequence = dynamic_cast<MathSequence *>(box)) {
    pos = insert(pos, sequence, 0, sequence->length());
    sequence->safe_destroy();
    return pos;
  }
  
  ensure_boxes_valid();
  
  spans_invalid = true;
  boxes_invalid = true;
  uint16_t ch = PMATH_CHAR_BOX;
  str.insert(pos, &ch, 1);
  adopt(box, pos);
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < pos)
    ++i;
  boxes.insert(i, 1, &box);
  invalidate();
  return pos + 1;
}

void MathSequence::remove(int start, int end) {
  ensure_boxes_valid();
  
  spans_invalid = true;
  
  int i = 0;
  while(i < boxes.length() && boxes[i]->index() < start)
    ++i;
    
  int j = i;
  while(j < boxes.length() && boxes[j]->index() < end)
    boxes[j++]->safe_destroy();
    
  boxes_invalid = i < boxes.length();
  boxes.remove(i, j - i);
  str.remove(start, end - start);
  invalidate();
}

Box *MathSequence::remove(int *index) {
  remove(*index, *index + 1);
  return this;
}

Box *MathSequence::extract_box(int boxindex) {
  Box *box = boxes[boxindex];
  
  DummyBox *dummy = new DummyBox();
  adopt(dummy, box->index());
  boxes.set(boxindex, dummy);
  
  abandon(box);
  return box;
}

////} ... insert/remove

template <class T>
static Box *create_or_error(Expr expr, BoxInputFlags options) {
  if(auto box = Box::try_create<T>(expr, options))
    return box;
    
  return new ErrorBox(expr);
}

static Box *create_box(Expr expr, BoxInputFlags options) {
  if(expr.is_string()) {
    InlineSequenceBox *box = new InlineSequenceBox;
    box->content()->load_from_object(expr, options);
    return box;
  }
  
  if(!expr.is_expr())
    return new ErrorBox(expr);
    
  Expr head = expr[0];
  
  if(head == PMATH_SYMBOL_LIST || head == richmath_System_ComplexStringBox) {
    if(expr.expr_length() == 1) {
      expr = expr[1];
      return create_box(expr, options);
    }
    
    InlineSequenceBox *box = new InlineSequenceBox;
    box->content()->load_from_object(expr, options);
    return box;
  }
  
  if(head == richmath_System_ButtonBox)
    return create_or_error<  ButtonBox>(expr, options);
    
  if(head == richmath_System_CheckboxBox)
    return create_or_error<  CheckboxBox>(expr, options);
    
  if(head == richmath_System_DynamicBox)
    return create_or_error<  DynamicBox>(expr, options);
    
  if(head == richmath_System_DynamicLocalBox)
    return create_or_error<  DynamicLocalBox>(expr, options);
    
  if(head == richmath_System_FillBox)
    return create_or_error<  FillBox>(expr, options);
    
  if(head == richmath_System_FractionBox)
    return create_or_error<  FractionBox>(expr, options);
    
  if(head == richmath_System_FrameBox)
    return create_or_error<  FrameBox>(expr, options);
    
  if(head == richmath_System_GraphicsBox)
    return create_or_error<  GraphicsBox>(expr, options);
    
  if(head == richmath_System_GridBox)
    return create_or_error<  GridBox>(expr, options);
    
  if(head == richmath_System_InputFieldBox)
    return create_or_error<  InputFieldBox>(expr, options);
    
  if(head == richmath_System_InterpretationBox)
    return create_or_error<  InterpretationBox>(expr, options);
    
  if(head == richmath_System_PanelBox)
    return create_or_error<  PanelBox>(expr, options);
    
  if(head == richmath_System_PaneSelectorBox)
    return create_or_error<  PaneSelectorBox>(expr, options);
    
  if(head == richmath_System_ProgressIndicatorBox)
    return create_or_error<  ProgressIndicatorBox>(expr, options);
    
  if(head == richmath_System_RadicalBox)
    return create_or_error<  RadicalBox>(expr, options);
    
  if(head == richmath_System_RadioButtonBox)
    return create_or_error<  RadioButtonBox>(expr, options);
    
  if(head == richmath_System_RotationBox)
    return create_or_error<  RotationBox>(expr, options);
    
  if(head == richmath_System_SetterBox)
    return create_or_error<  SetterBox>(expr, options);
    
  if(head == richmath_System_SliderBox)
    return create_or_error<  SliderBox>(expr, options);
    
  if(head == richmath_System_SubscriptBox)
    return create_or_error<  SubsuperscriptBox>(expr, options);
    
  if(head == richmath_System_SubsuperscriptBox)
    return create_or_error<  SubsuperscriptBox>(expr, options);
    
  if(head == richmath_System_SuperscriptBox)
    return create_or_error<  SubsuperscriptBox>(expr, options);
    
  if(head == richmath_System_SqrtBox)
    return create_or_error<  RadicalBox>(expr, options);
    
  if(head == richmath_System_StyleBox)
    return create_or_error<  StyleBox>(expr, options);
    
  if(head == richmath_System_TagBox)
    return create_or_error<  TagBox>(expr, options);
    
  if(head == richmath_System_TemplateBox)
    return create_or_error<  TemplateBox>(expr, options);
    
  if(head == richmath_System_TooltipBox)
    return create_or_error<  TooltipBox>(expr, options);
    
  if(head == richmath_System_TransformationBox)
    return create_or_error<  TransformationBox>(expr, options);
    
  if(head == richmath_System_OpenerBox)
    return create_or_error<  OpenerBox>(expr, options);
    
  if(head == richmath_System_OverscriptBox)
    return create_or_error<  UnderoverscriptBox>(expr, options);
    
  if(head == richmath_System_UnderoverscriptBox)
    return create_or_error<  UnderoverscriptBox>(expr, options);
    
  if(head == richmath_System_UnderscriptBox)
    return create_or_error<  UnderoverscriptBox>(expr, options);
    
  if(head == richmath_FE_NumberBox)
    return create_or_error<NumberBox>(expr, options);
    
  if(head == richmath_System_TemplateSlot)
    return create_or_error<TemplateBoxSlot>(expr, options);
    
  return new ErrorBox(expr);
}

class PositionedExpr {
  public:
    PositionedExpr()
      : pos(0)
    {
    }
    
    PositionedExpr(Expr _expr, int _pos)
      : expr(_expr),
        pos(_pos)
    {
    }
    
  public:
    Expr expr;
    int  pos;
};

static void defered_make_box(int pos, pmath_t obj, void *data) {
  Array<PositionedExpr> *boxes = (Array<PositionedExpr> *)data;
  
  boxes->add(PositionedExpr(Expr(obj), pos));
}

class SpanSynchronizer: public Base {
  public:
    SpanSynchronizer(
      BoxInputFlags             _new_load_options,
      Array<Box *>          &_old_boxes,
      SpanArray             &_old_spans,
      Array<PositionedExpr> &_new_boxes,
      SpanArray             &_new_spans
    ) : Base(),
      old_boxes(       _old_boxes),
      old_spans(       _old_spans),
      old_pos(         0),
      old_next_box(    0),
      new_load_options(_new_load_options),
      new_boxes(       _new_boxes),
      new_spans(       _new_spans),
      new_pos(         0),
      new_next_box(    0)
    {
      SET_BASE_DEBUG_TAG(typeid(*this).name());
    }
    
    bool is_in_range() {
      if(old_pos >= old_spans.length())
        return false;
        
      if(old_next_box >= old_boxes.length())
        return false;
        
      if(new_pos >= new_spans.length())
        return false;
        
      if(new_next_box >= new_boxes.length())
        return false;
        
      return true;
    }
    
    void next() {
      if(is_in_range())
        next(old_spans[old_pos], new_spans[new_pos]);
    }
    
    void finish() {
      if(old_pos == old_spans.length()) {
        assert(old_next_box == old_boxes.length());
      }
      
      if(new_pos == new_spans.length()) {
        assert(new_next_box == new_boxes.length());
      }
      
      int rem = 0;
      while(old_next_box + rem < old_boxes.length() &&
            old_boxes[old_next_box + rem]->index() < old_spans.length())
      {
        ++rem;
      }
      
      if(rem > 0) {
        for(int i = 0; i < rem; ++i)
          old_boxes[old_next_box + i]->safe_destroy();
          
        old_boxes.remove(old_next_box, rem);
      }
      
      while(new_next_box < new_boxes.length()) {
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        assert(new_box.pos < new_spans.length());
        
        Box *box = create_box(new_box.expr, new_load_options);
        old_boxes.insert(old_next_box, 1, &box);
        
        ++old_next_box;
        ++new_next_box;
      }
    }
    
  protected:
    void next(Span old_span, Span new_span) {
      if(!is_in_range()) {
        old_pos = old_spans.length();
        new_pos = new_spans.length();
        return;
      }
      
      if(old_span && new_span) {
        int old_start = old_pos;
        int new_start = new_pos;
        
        next(old_span.next(), new_span.next());
        
        assert(old_start < old_pos);
        assert(new_start < new_pos);
        
        while(old_pos <= old_span.end() &&
              new_pos <= new_span.end())
        {
          next(old_spans[old_pos], new_spans[new_pos]);
        }
      }
      
      if(old_span) {
        old_pos = old_span.end() + 1;
      }
      else {
        while(!old_spans.is_token_end(old_pos))
          ++old_pos;
        ++old_pos;
      }
      
      if(new_span) {
        new_pos = new_span.end() + 1;
      }
      else {
        while(!new_spans.is_token_end(new_pos))
          ++new_pos;
        ++new_pos;
      }
      
      while(old_next_box < old_boxes.length() &&
            new_next_box < new_boxes.length())
      {
        Box *box = old_boxes[old_next_box];
        
        if(box->index() >= old_pos)
          break;
          
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        if(new_box.pos >= new_pos)
          break;
          
        if(!box->try_load_from_object(new_box.expr, new_load_options)) {
          box->safe_destroy();
          box = create_box(new_box.expr, new_load_options);
          
          old_boxes.set(old_next_box, box);
        }
        
        ++old_next_box;
        ++new_next_box;
      }
      
      int rem = 0;
      while(old_next_box + rem < old_boxes.length() &&
            old_boxes[old_next_box + rem]->index() < old_pos)
      {
        ++rem;
      }
      
      if(rem > 0) {
        for(int i = 0; i < rem; ++i)
          old_boxes[old_next_box + i]->safe_destroy();
          
        old_boxes.remove(old_next_box, rem);
      }
      
      while(new_next_box < new_boxes.length()) {
        PositionedExpr &new_box = new_boxes[new_next_box];
        
        if(new_box.pos >= new_pos)
          break;
          
        Box *box = create_box(new_box.expr, new_load_options);
        old_boxes.insert(old_next_box, 1, &box);
        
        ++old_next_box;
        ++new_next_box;
      }
    }
    
  public:
    Array<Box *>     &old_boxes;
    const SpanArray &old_spans;
    int              old_pos;
    int              old_next_box;
    
    BoxInputFlags                   new_load_options;
    const Array<PositionedExpr> &new_boxes;
    const SpanArray             &new_spans;
    int                          new_pos;
    int                          new_next_box;
};

void MathSequence::load_from_object(Expr object, BoxInputFlags options) {
  ensure_boxes_valid();
  
  Array<PositionedExpr> new_boxes;
  pmath_string_t        new_string;
  SpanArray             new_spans;
  
  Expr obj = object;
  
  if(obj[0] == richmath_System_BoxData && obj.expr_length() == 1)
    obj = obj[1];
    
  if(has(options, BoxInputFlags::FormatNumbers))
    obj = NumberBox::prepare_boxes(obj);
    
  if(has(options, BoxInputFlags::AllowTemplateSlots))
    obj = TemplateBoxSlot::prepare_boxes(obj);
    
  new_spans = pmath_spans_from_boxes(
                pmath_ref(obj.get()),
                &new_string,
                defered_make_box,
                &new_boxes);
                
  SpanSynchronizer syncer(options, boxes, spans, new_boxes, new_spans);
  
  while(syncer.is_in_range())
    syncer.next();
  syncer.finish();
  
  spans         = new_spans.extract_array();
  str           = String(new_string);
  boxes_invalid = true;
  
  finish_load_from_object(std::move(object));
}

bool MathSequence::stretch_horizontal(Context *context, float width) {
  if(glyphs.length() != 1 || str[0] == PMATH_CHAR_BOX)
    return false;
    
  if(context->math_shaper->horizontal_stretch_char(
        context,
        width,
        str[0],
        &glyphs[0]))
  {
    _extents.width = glyphs[0].right;
    _extents.ascent  = _extents.descent = -1e9;
    context->math_shaper->vertical_glyph_size(
      context, str[0], glyphs[0], &_extents.ascent, &_extents.descent);
    lines[0].ascent  = _extents.ascent;
    lines[0].descent = _extents.descent;
    return true;
  }
  return false;
}

int MathSequence::get_line(int index, int guide) {
  if(guide >= lines.length())
    guide = lines.length() - 1;
  if(guide < 0)
    guide = 0;
    
  int line = guide;
  
  if(line < lines.length() && lines[line].end > index) {
    while(line > 0) {
      if(lines[line - 1].end <= index)
        return line;
        
      --line;
    }
    
    return 0;
  }
  
  while(line < lines.length()) {
    if(lines[line].end > index)
      return line;
      
    ++line;
  }
  
  return lines.length() > 0 ? lines.length() - 1 : 0;
}

void MathSequence::get_line_heights(int line, float *ascent, float *descent) {
  if(length() == 0) {
    *ascent  = 0.75f * em;
    *descent = 0.25f * em;
    return;
  }
  
  if(line < 0 || line >= lines.length()) {
    *ascent = *descent = 0;
    return;
  }
  
  *ascent  = lines[line].ascent;
  *descent = lines[line].descent;
}

int MathSequence::get_box(int index, int guide) {
  assert(str[index] == PMATH_CHAR_BOX);
  
  ensure_boxes_valid();
  if(guide < 0)
    guide = 0;
    
  for(int box = guide; box < boxes.length(); ++box) {
    if(boxes[box]->index() == index)
      return box;
  }
  
  
  if(guide >= boxes.length())
    guide = boxes.length();
    
  for(int box = 0; box < guide; ++box) {
    if(boxes[box]->index() == index)
      return box;
  }
  
  assert(0 && "no box found at index.");
  return -1;
}

float MathSequence::indention_width(int i) {
  float f = i * em / 2;
  
  if(f <= _extents.width / 2)
    return f;
    
  return floor(_extents.width / em) * em / 2;
}

//} ... class MathSequence

