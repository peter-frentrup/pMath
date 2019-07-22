#include <util/selection-tracking.h>

#include <boxes/mathsequence.h>
#include <boxes/numberbox.h>
#include <boxes/section.h>
#include <boxes/sectionlist.h>
#include <boxes/textsequence.h>

#include <util/spanexpr.h>

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>


using namespace richmath;
using namespace pmath;

namespace {
  class TrackedSelection {
    public:
      TrackedSelection() = default;
      
      TrackedSelection(const LocationReference &loc) 
        : location{loc},
          token_output_start{-1},
          token_output_depth{-1},
          output_pos{-1},
          token_source_start{-1},
          token_source_end{-1}
      {
      }
      
      void pre_write(pmath_t obj, const SelectionReference &source, const String &output, int call_depth) {
        if(source.id == location.id) {
          if(source.start <= location.index && location.index <= source.end) {
            int written = output.length();
            if(token_output_start <= written) {
              token_output_start = written;
              token_output_depth = call_depth;
              token              = Expr{ pmath_ref(obj) };
              token_source_start = source.start;
              token_source_end   = source.end;
            }
          }
        }
        else if(pmath_is_string(obj)) {
          Box *box_at_source = find_box_at(source);
          
          if(NumberBox *num = dynamic_cast<NumberBox*>(box_at_source)) {
            if(num->is_number_part(location.get())) {
              int written = output.length();
              if(token_output_start <= written) {
                token_output_start = written;
                token_output_depth = call_depth;
                token              = Expr{ pmath_ref(obj) };
                token_source_start = source.start;
                token_source_end   = source.end;
              }
            }
          }
        }
      }
      
      void post_write(pmath_t obj, const SelectionReference &source, const String &output, int call_depth) {
        if(call_depth != token_output_depth) 
          return;
          
        token_output_depth = -1;
        int token_output_end = output.length();
        if(token_output_end < token_output_start)
          return;
        
        const uint16_t *buf = output.buffer();
        
        int out_tok_len = token_output_end - token_output_start;
        int in_tok_len  = token_source_end - token_source_start;
        
        const uint16_t *in16 = nullptr;
        const char     *in8 = nullptr;
        int in_length = 0;
        int in_start = token_source_start;
        int in_index = location.index;
        
        Box *selbox = location.get();
        if(source.id == location.id) {
          if(auto mseq = dynamic_cast<MathSequence*>(selbox)) {
            in16 = mseq->text().buffer();
            in_length = mseq->length();
          }
          else if(auto tseq = dynamic_cast<TextSequence*>(selbox)) {
            in8 = tseq->text_buffer().buffer();
            in_length = tseq->length();
          }
          else {
            output_pos = token_output_start;
            return;
          }
        }
        else if(pmath_is_string(obj)){
          Box *box_at_source = find_box_at(source);
          
          if(NumberBox *num = dynamic_cast<NumberBox*>(box_at_source)) {
            PositionInRange pos = num->selection_to_string_index(String{pmath_ref(obj)}, selbox, location.index);
            if(pos.pos >= 0) {
              pos.pos = std::max(pos.start, std::min(pos.pos, pos.end));
            }
            if(pos.pos <= out_tok_len) {
              //output_pos = token_output_start + pos.pos;
              //return;
              in16 = pmath_string_buffer((pmath_string_t*)&obj);
              in_length = pmath_string_length((pmath_string_t)obj);
              in_start = 0;
              in_index = pos.pos;
            }
            else if(num->is_number_part(selbox)) {
              output_pos = token_output_start;
              return;
            }
          }
        }
        
        if( out_tok_len >= in_tok_len + 2 && 
            in_index <= in_length &&
            buf[token_output_start] == '"' &&
            buf[token_output_end - 1] == '"') 
        {
          int opos = token_output_start + 1;
          int ipos = in_start;
          while(ipos < in_index && opos < token_output_end - 1) {
            if(in8) {
              if(in8[ipos]) {
                const char *next = g_utf8_find_next_char(in8 + ipos, in8 + in_index);
                if(next)
                  ipos = (int)(next - in8);
                else
                  ipos = in_index;
              }
              else
                ++ipos; // embedded <NUL>
            }
            else if(in16) {
              if( is_utf16_high(in16[ipos]) && 
                  ipos + 1 < in_index &&
                  is_utf16_low(in16[ipos + 1])) 
              {
                ipos+= 2;
              }
              else
                ++ipos;
            }
            else
              break;
            
            if(buf[opos] == '\\') {
              ++opos;
              if(opos < token_output_end - 1 && buf[opos] == '[') {
                while(opos < token_output_end - 1 && buf[opos] != ']')
                  ++opos;
                
                ++opos;
              }
              else
                ++opos;
            }
            else if(is_utf16_high(buf[opos]) && 
                opos + 1 < token_output_end && 
                is_utf16_low(buf[opos + 1]))
            {
              opos+= 2;
            }
            else
              ++opos;
          }
          
          output_pos = opos;
        }
      }
      
    private:
      static Box *find_box_at(const SelectionReference &source) {
        FrontEndObject *feo = FrontEndObject::find(source.id);
        if(!feo)
          return nullptr;
        
        if(MathSequence *mseq = dynamic_cast<MathSequence*>(feo)) {
          if(source.end != source.start + 1)
            return nullptr;
          if(source.start < 0 || source.start >= mseq->length())
            return nullptr;
          
          if(mseq->char_at(source.start) != PMATH_CHAR_BOX)
            return nullptr;
          
          return raw_find_box_at(mseq, source.start);
        }
        else if(TextSequence *tseq = dynamic_cast<TextSequence*>(feo)) {
          if(!tseq->text_buffer().is_box_at(source.start, source.end)) 
            return nullptr;
          
          return raw_find_box_at(tseq, source.start);
        }
        else if(Box *box = dynamic_cast<Box*>(feo)) {
          if(source.start == 0 && source.end == box->length())
            return box;
        }
        
        return nullptr;
      }
      
      static Box *raw_find_box_at(Box *parent, int index) {
        for(int i = parent->count(); i > 0; --i) {
          Box *box = parent->item(i - 1);
          int box_index = box->index();
          if(box_index == index)
            return box;
          if(box_index < index)
            return nullptr;
        }
        return nullptr;
      }
    
    public:
      LocationReference location;
      
      Expr  token;
      int   token_output_start;
      int   token_output_depth;
      int   output_pos;
      int   token_source_start;
      int   token_source_end;
  };
  
  class PrintTracking {
    public:
      PrintTracking() 
        : output{""},
          call_depth{0}
      {
      }
      
      String write(Expr obj, pmath_write_options_t options) {
        pmath_write_ex_t info = {0};
        info.size = sizeof(info);
        info.options = options;
        info.user = this;
        info.write = write_callback;
        info.pre_write = pre_write_callback;
        info.post_write = post_write_callback;
        
        output = String("");
        pmath_write_ex(&info, obj.get());
        return output;
      }
      
    private:
      static void write_callback(void *_self, const uint16_t *data, int len) {
        PrintTracking *self = (PrintTracking*)_self;
        self->write(data, len);
      };
      
      static void pre_write_callback(void *_self, pmath_t obj, pmath_write_options_t opts) {
        PrintTracking *self = (PrintTracking*)_self;
        self->pre_write(obj, opts);
      };
      
      static void post_write_callback(void *_self, pmath_t obj, pmath_write_options_t opts) {
        PrintTracking *self = (PrintTracking*)_self;
        self->post_write(obj, opts);
      };
    
    private:
      void write(const uint16_t *data, int len) {
        output.insert(INT_MAX, data, len);
      }
      
      void pre_write(pmath_t obj, pmath_write_options_t opts) {
        ++call_depth;
        
        SelectionReference source = SelectionReference::from_debug_info_of(obj);
        if(source) {
          for(auto &sel : selections)
            sel.pre_write(obj, source, output, call_depth);
        }
      }
      
      void post_write(pmath_t obj, pmath_write_options_t opts) {
        SelectionReference source = SelectionReference::from_debug_info_of(obj);
        if(source) {
          for(auto &sel : selections)
            sel.post_write(obj, source, output, call_depth);
        }
        
        --call_depth;
      }
    
    public:
      String                  output;
      Array<TrackedSelection> selections;
    
    private:
      int                     call_depth;
  };
  
  class TrackedBoxSelection {
    public:
      TrackedBoxSelection() = default;
      
      TrackedBoxSelection(const LocationReference &loc) 
        : source_location{loc}
      {
      }
      
      void after_creation(Box *box, const SelectionReference &source, Expr expr) {
        if(source.id != source_location.id)
          return;
        
        if(destination) {
          Box *tmp = box->parent();
          while(tmp && tmp->id() != destination.id)
            tmp = tmp->parent();
            
          if(!tmp)
            return;
        }
        
        if(source_location.index < source.start || source.end < source_location.index)
          return;
        
        if(auto seq = dynamic_cast<MathSequence*>(box)) {
          track_math_sequence(seq, source, std::move(expr));
        }
        else if(auto seq = dynamic_cast<TextSequence*>(box)) {
          track_text_sequence(seq, source, std::move(expr));
        }
        else if(auto num = dynamic_cast<NumberBox*>(box)) {
          if(auto source_seq = FrontEndObject::find_cast<MathSequence>(source.id)) {
            if(0 <= source.start && source.start <= source.end && source.end <= source_seq->length()) {
              String number = source_seq->text().part(source.start, source.end - source.start);
              
              int index;
              Box *selbox = num->string_index_to_selection(number, source_location.index - source.start, &index);
              if(selbox) {
                destination.set(selbox, index, index);
                return;
              }
            }
          }
        }
      }
    
    private:
      void track_math_sequence(MathSequence *seq, const SelectionReference &source, Expr expr) {
        int len = seq->length();
        if(len == 0) {
          destination.set(seq, 0, 0);
          return;
        }
        
        SpanExpr *se = new SpanExpr(0, seq->span_array()[0], seq);
        if(se->end() + 1 != len) {
          if(expr.is_expr() && expr[0] == PMATH_NULL) {
            for(size_t i = 1; ;++i) {
              Expr item = expr[i];
              SelectionReference item_source = SelectionReference::from_debug_info_of(item);
              if(source_location.index <= item_source.end && item_source.id == source_location.id) {
                visit_span(se, item_source, item);
                delete se;
                return;
              }
              
              int pos = se->end() + 1;
              if(pos < len) {
                delete se;
                se = new SpanExpr(pos, seq->span_array()[pos], seq);
              }
              else
                break;
            }
          }
        }
        visit_span(se, source, std::move(expr));
        delete se;
      }
      
      void visit_span(SpanExpr *se, const SelectionReference &source, Expr expr) {
        if(source_location.index <= source.start) {
          destination.set(se->sequence(), se->start(), se->start());
          return;
        }
        else if(source_location.index >= source.end) {
          destination.set(se->sequence(), se->end() + 1, se->end() + 1);
          return;
        }
        
        size_t expr_len = expr.expr_length();
        int count = se->count();
        
        if(expr_len > 0 && (size_t)count == expr_len && (expr[0] == PMATH_NULL || expr[0] == PMATH_SYMBOL_LIST)) {
          for(int i = 0; i < count; ++i) {
            Expr item = expr[(size_t)i + 1];
            SelectionReference item_source = SelectionReference::from_debug_info_of(item);
            
            if(source_location.index <= item_source.end && item_source.id == source_location.id) {
              visit_span(se->item(i), item_source, item);
              return;
            }
          }
          
          destination.set(se->sequence(), se->end() + 1, se->end() + 1);
          return;
        }
        
        if(count == 0 && expr.is_string()) {
          if(se->as_text() == expr) {
            const uint16_t *se_buf = se->sequence()->text().buffer();
            
            if(auto source_seq = FrontEndObject::find_cast<MathSequence>(source.id)) {
              if(0 <= source.start && source.start <= source.end && source.end <= source_seq->length()) {
                const uint16_t *buf = source_seq->text().buffer();
                
                if(source.end - source.start >= 2 && buf[source.start] == '"' && buf[source.end-1] == '"') {
                  int in_pos = source.start + 1;
                  int o_pos = se->start();
                  int o_pos_max = se->end();
                  while(o_pos <= o_pos_max && in_pos < source_location.index) {
                    int in_next = next_char_pos(buf, in_pos, source.end);
                    
                    if(source_location.index < in_next) {
                      destination.set(se->sequence(), o_pos, o_pos + 1);
                      return;
                    }
                    in_pos = in_next;
                    ++o_pos;
                    if(o_pos < o_pos_max && is_utf16_high(se_buf[o_pos-1]) && is_utf16_low(se_buf[o_pos]))
                      ++o_pos;
                  }
                  
                  destination.set(se->sequence(), o_pos, o_pos);
                  return;
                }
              }
            }
          }
        }
        
        destination.set(se->sequence(), se->start(), se->end() + 1);
      }
      
      static int next_char_pos(const uint16_t *buf, int pos, int end) {
        if(buf[pos] == '\\' && pos + 1 < end) {
          ++pos;
          if(buf[pos] == '[') {
            while(pos < end && buf[pos - 1] != ']') {
              ++pos;
            }
          }
          else
            ++pos;
        }
        else
          ++pos;
        
        return pos;
      }
      
      static bool is_utf8_continuation(char c) {
        return ((unsigned char)c & 0xC0) == 0x80;
      }
      
      void track_text_sequence(TextSequence *seq, const SelectionReference &source, Expr expr) {
        if(expr.is_string()) {
          visit_text_span(seq, 0, seq->length(), source, std::move(expr));
          return;
        }
        
        if(expr[0] == PMATH_SYMBOL_LIST) {
          const char *o_buf = seq->text_buffer().buffer();
          int o_pos = 0;
          int o_len = seq->length();
          int boxi = 0;
          
          for(auto item: expr.items()) {
            if(o_pos >= o_len)
              break;
            
            SelectionReference item_source = SelectionReference::from_debug_info_of(item);

            int o_next = o_pos + 1;
            if(boxi < seq->count()) {
              if(boxi < seq->count() && seq->item(boxi)->index() == o_pos) {
                ++boxi;
                while(o_next < o_len && is_utf8_continuation(o_buf[o_next]))
                  ++o_next;
              }
              else
                o_next = seq->item(boxi)->index();
            }
            else
              o_next = o_len;
            
            if(source_location.index <= item_source.end && item_source.id == source_location.id) {
              visit_text_span(seq, o_pos, o_next, item_source, std::move(item));
              return;
            }
            
            o_pos = o_next;
          }
        }
        
        destination.set(seq, 0, seq->length());
      }
      
      void visit_text_span(TextSequence *seq, int start, int end, const SelectionReference &source, Expr expr) {
        if(auto source_seq = FrontEndObject::find_cast<MathSequence>(source.id)) {
          if(0 <= source.start && source.start <= source.end && source.end <= source_seq->length()) {
            const uint16_t *buf = source_seq->text().buffer();
            
            if(source.end - source.start >= 2 && buf[source.start] == '"' && buf[source.end-1] == '"') {
              const char *o_buf = seq->text_buffer().buffer();
              
              int in_pos = source.start + 1;
              int o_pos = start;
              int o_pos_max = end;
              while(o_pos <= o_pos_max && in_pos < source_location.index) {
                int in_next = next_char_pos(buf, in_pos, source.end);
                
                if(source_location.index < in_next) {
                  destination.set(seq, o_pos, o_pos + 1);
                  return;
                }
              
                in_pos = in_next;
                ++o_pos;
                while(o_pos < o_pos_max && is_utf8_continuation(o_buf[o_pos]))
                  ++o_pos;
              }
              
              destination.set(seq, o_pos, o_pos);
              return;
            }
          }
        }
        
        destination.set(seq, start, end);
      }
      
    public:
      LocationReference source_location;
      
      SelectionReference destination;
  };
  
  class BoxTracking {
    public:
      BoxTracking() 
        : callback {
            [this](Box *box, Expr expr){ after_creation(box, std::move(expr)); }, 
            Box::on_finish_load_from_object}
      {
      }
      
      template<class F>
      void for_new_boxes_in(F body) {
        callback.next = Box::on_finish_load_from_object;
        Box::on_finish_load_from_object = &callback;
        
        body();
        
        Box::on_finish_load_from_object = callback.next;
      }
    
    private:
      void after_creation(Box *box, Expr expr) {
        auto source = SelectionReference::from_debug_info_of(expr);
        if(source)
          for(auto &sel : selections)
            sel.after_creation(box, source, expr);
      }
      
    public:
      Array<TrackedBoxSelection> selections;
    
    private:
      FunctionChain<Box*, Expr> callback;
  };
}

static bool begin_edit_section(
  Section *section, 
  const Array<LocationReference> &old_locations,
  Hashtable<LocationReference, SelectionReference> &found_locations);
  
static bool finish_edit_section(
  EditSection *edit, 
  const Array<LocationReference> &old_locations,
  Hashtable<LocationReference, SelectionReference> &found_locations);

static void mask_box_chars(String &string);
  
bool richmath::toggle_edit_section(
  Section *section, 
  const Array<LocationReference> &old_locations,
  Hashtable<LocationReference, SelectionReference> &found_locations
) {
  if(auto edit = dynamic_cast<EditSection*>(section))
    return finish_edit_section(edit, old_locations, found_locations);
  else if(section)
    return begin_edit_section(section, old_locations, found_locations);
  else
    return false;
}

static bool begin_edit_section(
  Section *section, 
  const Array<LocationReference> &old_locations,
  Hashtable<LocationReference, SelectionReference> &found_locations
) {
  SectionList *parent = dynamic_cast<SectionList*>(section->parent());
  if(!parent)
    return false;
  
  int index = section->index();
    
  EditSection *edit = new EditSection;
  if(!edit->style)
    edit->style = new Style;
  edit->style->set(SectionGroupPrecedence, section->get_style(SectionGroupPrecedence));
  edit->swap_id(section);
  
  Expr obj(section->to_pmath(BoxOutputFlags::WithDebugInfo));
  
  //Expr tmp = Call(Symbol(PMATH_SYMBOL_FULLFORM), obj);
  //pmath_debug_print_object("\n fullform: ", tmp.get(), "\n");
  
  PrintTracking pt;
  pt.selections.add(old_locations.length(), old_locations.items());
  
  pt.write(obj, 
           PMATH_WRITE_OPTIONS_FULLSTR | 
           PMATH_WRITE_OPTIONS_INPUTEXPR | 
           PMATH_WRITE_OPTIONS_FULLNAME_NONSYSTEM | 
           PMATH_WRITE_OPTIONS_NOSPACES | 
           PMATH_WRITE_OPTIONS_PREFERUNICODE);
  
  mask_box_chars(pt.output);
  edit->content()->insert(0, pt.output);
  
  edit->original = parent->swap(index, edit);
  
  for(const auto &sel : pt.selections) {
    if(sel.output_pos >= 0)
      found_locations.set(sel.location, SelectionReference{edit->content(), sel.output_pos, sel.output_pos});
  }
  
  return true;
}
  
static bool finish_edit_section(
  EditSection *edit, 
  const Array<LocationReference> &old_locations,
  Hashtable<LocationReference, SelectionReference> &found_locations
) {
  SectionList *parent = dynamic_cast<SectionList*>(edit->parent());
  if(!parent)
    return false;
  
  int index = edit->index();
    
  Expr parsed(edit->to_pmath(BoxOutputFlags::WithDebugInfo));
  
  if(parsed.is_null()) 
    return false;
  
  BoxTracking bt;
  
  bt.selections.add(old_locations.length(), old_locations.items());
  
  bt.for_new_boxes_in([&] {
    Section *sect = edit->original;
    if(sect && sect->try_load_from_object(parsed, BoxInputFlags::Default)) {
      edit->original = nullptr;
    }
    else
      sect = Section::create_from_object(parsed);
      
    sect->swap_id(edit);
    parent->swap(index, sect)->safe_destroy();
  });
  
  
  for(const auto &sel : bt.selections) {
    if(sel.destination)
      found_locations.set(sel.source_location, sel.destination);
  }
  
  return true;
}

static void mask_box_chars(String &string) {
  static const uint16_t MaskChar = 0xFFFC;
  static_assert(PMATH_CHAR_BOX != MaskChar, "invalid masking character");
  
  const uint16_t *buf = string.buffer();
  int len = string.length();
  
  int first_box = 0;
  while(first_box < len && buf[first_box] != PMATH_CHAR_BOX)
    ++first_box;
  
  if(first_box == len)
    return;
  
  pmath_string_t str = (pmath_string_t)string.release();
  uint16_t *outbuf;
  if(pmath_string_begin_write(&str, &outbuf, &len)) {
    for(int i = first_box;i < len;++i) {
      if(outbuf[i] == PMATH_CHAR_BOX)
        outbuf[i] = MaskChar;
    }
    
    pmath_string_end_write(&str, &outbuf);
    string = String{ str };
  }
}
