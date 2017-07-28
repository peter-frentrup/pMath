#include <boxes/numberbox.h>

#include <cfloat>
#include <cmath>
#include <cstdio>

#include <boxes/mathsequence.h>
#include <boxes/subsuperscriptbox.h>
#include <eval/binding.h>
#include <graphics/context.h>

#ifdef _MSC_VER
#  define snprintf sprintf_s
#endif


using namespace richmath;


namespace {
  static const uint16_t MULTIPLICATION_SPACE_CHAR = 0x2006;
  static const uint16_t TIMES_CHAR = 0x00D7;
  
  
  struct NumberPartPositions {
    String _number;
    
    int mid_mant_start;
    int mid_mant_end;
    int radius_mant_start;
    int radius_mant_end;
    int radius_exp_start;
    int radius_exp_end;
    int prec_start;
    int prec_end;
    int exp_start;
    int exp_end;
    
    explicit NumberPartPositions(String number) {
      _number = number;
      
      const uint16_t *buf = _number.buffer();
      const int       len = _number.length();
      
      mid_mant_start = 0;
      while(mid_mant_start < len && pmath_char_is_digit(buf[mid_mant_start])) 
        ++mid_mant_start;
      
      if( mid_mant_start + 1 < len        &&
          buf[mid_mant_start]     == '^' &&
          buf[mid_mant_start + 1] == '^')
      {
        mid_mant_start += 2;
      }
      else {
        mid_mant_start = 0;
      }
      
      mid_mant_end = mid_mant_start;
      while(mid_mant_end < len && buf[mid_mant_end] != '`' && buf[mid_mant_end] != '[')
        ++mid_mant_end;
        
      if(mid_mant_end < len && buf[mid_mant_end] == '[') {
        radius_mant_start = mid_mant_end;
        while(radius_mant_start < len && !pmath_char_is_36digit(buf[radius_mant_start]))
          ++radius_mant_start;
          
        // TODO: support "p" or "e" exponent specifiers
        radius_mant_end = radius_mant_start;
        while(radius_mant_end < len && buf[radius_mant_end] != '*' && buf[radius_mant_end] != ']')
          ++radius_mant_end;
          
        radius_exp_start = radius_mant_end;
        if(radius_exp_start + 1 < len && buf[radius_mant_end] == '*' && buf[radius_mant_end + 1] == '^')
          radius_exp_start += 2;
          
        radius_exp_end = radius_exp_start;
        while(radius_exp_end < len && buf[radius_exp_end] != ']')
          ++radius_exp_end;
      }
      else {
        radius_mant_start = mid_mant_end;
        radius_mant_end = mid_mant_end;
        radius_exp_start = mid_mant_end;
        radius_exp_end = mid_mant_end;
      }
      
      prec_start = radius_exp_end;
      while(prec_start < len && buf[prec_start] == '`')
        ++prec_start;
        
      // TODO: support "p" or "e" exponent specifiers
      prec_end = prec_start;
      while(prec_end < len && buf[prec_end] != '*')
        ++prec_end;
        
      exp_start = prec_end;
      if(exp_start + 1 < len && buf[exp_start] == '*' && buf[exp_start + 1] == '^')
        exp_start += 2;
        
      exp_end = len;
    }
    
    bool has_radix() { return mid_mant_start > 2; }
    int radix_end() { return (mid_mant_start > 2) ? mid_mant_start - 2 : 0; }
    String radix() { return (mid_mant_start > 2) ? _number.part(0, radix_end()) : String(); } // TODO: support "0x" base as "16^^"
    
    String midpoint_mantissa() { return _number.part(mid_mant_start, mid_mant_end - mid_mant_start); }
    
    bool has_radius() { return radius_mant_start < radius_mant_end; }
    String radius_mantissa() { return _number.part(radius_mant_start, radius_mant_end - radius_mant_start); }
    
    bool has_radius_exponent() { return radius_exp_start < radius_exp_end; }
    String radius_exponent() { return _number.part(radius_exp_start, radius_exp_end - radius_exp_start); }
    
    bool has_exponent() { return exp_start < exp_end; }
    String exponent() { return _number.part(exp_start, exp_end - exp_start); }
  };
  
  struct PositionInRange {
    int pos;
    int start;
    int end;
    
    PositionInRange(int _pos, int _start, int _end)
      : pos(_pos),
        start(_start),
        end(_end)
    {
    }
    
    bool is_valid() { return start <= pos && pos <= end; }
  };
  
}

namespace richmath {
  class NumberBoxImpl {
    private:
      NumberBox &self;
      
    public:
      NumberBoxImpl(NumberBox &_self)
        : self(_self)
      {
      }
      
    private:
      void append(uint16_t chr) {
        self._content->insert(self._content->length(), chr);
      }
      
      void append(const uint16_t *ucs2, int len) {
        self._content->insert(self._content->length(), ucs2, len);
      }
      
      void append(const char *latin1, int len) {
        self._content->insert(self._content->length(), latin1, len);
      }
      
      void append(String s) {
        self._content->insert(self._content->length(), s);
      }
      
      void append(Box *box) {
        self._content->insert(self._content->length(), box);
      }
      
      MathSequence *append_radix(NumberPartPositions &parts) {
        if(!parts.has_radix())
          return nullptr;
          
        SubsuperscriptBox *bas = new SubsuperscriptBox(new MathSequence, 0);
        MathSequence *seq = bas->subscript();
        seq->insert(0, parts.radix());
        append(bas);
        return seq;
      }
      
      MathSequence *append_superscript(NumberPartPositions &parts, String s) {
        SubsuperscriptBox *exp = new SubsuperscriptBox(0, new MathSequence);
        MathSequence *seq = exp->superscript();
        seq->insert(0, s);
        
        const uint16_t times[3] = { MULTIPLICATION_SPACE_CHAR, TIMES_CHAR, MULTIPLICATION_SPACE_CHAR };
        append(times, 3);
        
        if(parts.has_radix())
          append(parts.radix());
        else
          append("10");
          
        append(exp);
        return exp->superscript();
      }
      
    public:
      void set_number(String n) {
        self._number = n;
        self._base = nullptr;
        self._radius_base = nullptr;
        self._radius_exponent = nullptr;
        self._exponent = nullptr;
        
        NumberPartPositions parts{ n };
        
        self._content->remove(0, self._content->length());
        
        append(parts.midpoint_mantissa());
        self._base = append_radix(parts);
        
        if(parts.has_radius()) {
          const uint16_t opening[2] = { '[', PMATH_CHAR_PLUSMINUS };
          append(opening, 2);
          
          append(parts.radius_mantissa());
          self._radius_base = append_radix(parts);
          
          if(parts.has_radius_exponent())
            self._radius_exponent = append_superscript(parts, parts.radius_exponent());
            
          append(']');
        }
        
        if(parts.has_exponent())
          self._exponent = append_superscript(parts, parts.exponent());
      }
      
    public:
      PositionInRange selection_to_string_index(Box *selbox, int selpos) {
        NumberPartPositions parts{ self._number };
        
        if(selbox == nullptr)
          return PositionInRange(-1, 0, 0); // error
          
        if(selbox == self._exponent)
          return PositionInRange(parts.exp_start + selpos, parts.exp_start, parts.exp_end);
          
        if(selbox == self._radius_exponent)
          return PositionInRange(parts.radius_exp_start + selpos, parts.radius_exp_start, parts.radius_exp_end);
          
        if(selbox == self._base || selbox == self._radius_base)
          return PositionInRange(selpos, 0, parts.radix_end());
          
        // we should be in the midpoint mantissa, radius mantissa or one of the "[+/-", "]" or "*10" parts
        if(selbox != self._content)
          return PositionInRange(-1, 0, 0); // error
          
        int len = self._content->length();
        if(selpos <= 0)
          return PositionInRange(0, 0, parts.mid_mant_end);
          
        if(selpos >= len)
          return PositionInRange(self._number.length(), parts.exp_start, parts.exp_end);
          
        const uint16_t *buf = self._content->text().buffer();
        int i = selpos;
        while(i > 0 && (buf[i - 1] == '.' || pmath_char_is_36digit(buf[i - 1])))
          --i;
          
        if(i == 0)
          return PositionInRange(parts.mid_mant_start + selpos, parts.mid_mant_start, parts.mid_mant_end);
          
        if(buf[i - 1] == PMATH_CHAR_PLUSMINUS)
          return PositionInRange(parts.radius_mant_start + selpos - i, parts.radius_mant_start, parts.radius_mant_end);
          
        if(buf[i - 1] == '[')
          return PositionInRange(parts.mid_mant_end + selpos - i, parts.mid_mant_end + 1, parts.radius_mant_start);
          
        if(buf[i - 1] == MULTIPLICATION_SPACE_CHAR || buf[i - 1] == TIMES_CHAR) {
          // in onw of the two "*^" exponent specifiers
          int j = selpos;
          while(j < len && (buf[j] == '.' || pmath_char_is_36digit(buf[j]) || buf[j] == PMATH_CHAR_BOX))
            ++j;
            
          if(j == len)
            return PositionInRange(self._number.length(), parts.prec_end, parts.exp_start);
            
          if(buf[j] == ']')
            return PositionInRange(parts.radius_mant_end, parts.radius_mant_end, parts.radius_exp_start);
        }
        
        return PositionInRange(-1, 0, 0); // error
      }
  };
}

//{ class NumberBox ...

NumberBox::NumberBox()
  : OwnerBox()
{
  NumberBoxImpl(*this).set_number(String(""));
}

NumberBox::NumberBox(String number)
  : OwnerBox()
{
  NumberBoxImpl(*this).set_number(number);
}

bool NumberBox::try_load_from_object(Expr expr, BoxOptions opts) {
  if(expr[0] != GetSymbol(FESymbolIndex::NumberBox))
    return false;
    
  if(expr.expr_length() != 1)
    return false;
    
  String s(expr[1]);
  if(s.is_null())
    return false;
    
  NumberBoxImpl(*this).set_number(s);
  return true;
}

bool NumberBox::edit_selection(Context *context) {
  if(Box::edit_selection(context)) {
    auto seq = dynamic_cast<MathSequence*>(_parent);
    if(!seq)
      return false;
      
    Box *selbox = context->selection.get();
    PositionInRange pos_start = NumberBoxImpl(*this).selection_to_string_index(selbox, context->selection.start);
    PositionInRange pos_end = NumberBoxImpl(*this).selection_to_string_index(selbox, context->selection.end);
    
    if(!pos_start.is_valid() || !pos_end.is_valid())
      return false;
    
    int i = _index;
    
    seq->insert(_index + 1, _number);
    seq->remove(_index, _index + 1); // deletes this
    
    context->selection.set(seq, i + pos_start.pos, i + pos_end.pos);
    return true;
  }
  
  return false;
}

void NumberBox::resize(Context *context) {
  bool                  old_math_spacing     = context->math_spacing;
  bool                  old_show_auto_styles = context->show_auto_styles;
  SharedPtr<TextShaper> old_text_shaper      = context->text_shaper;
  
  context->math_spacing     = false;
  context->show_auto_styles = false;
  
  if(old_math_spacing)
    context->text_shaper = context->math_shaper;
    
  OwnerBox::resize(context);
  
  context->math_spacing     = old_math_spacing;
  context->show_auto_styles = old_show_auto_styles;
  context->text_shaper      = old_text_shaper;
}

void NumberBox::paint(Context *context) {
  bool                  old_math_spacing     = context->math_spacing;
  bool                  old_show_auto_styles = context->show_auto_styles;
  SharedPtr<TextShaper> old_text_shaper      = context->text_shaper;
  
  context->math_spacing     = false;
  context->show_auto_styles = false;
  
  if(old_math_spacing)
    context->text_shaper = context->math_shaper;
    
  OwnerBox::paint(context);
  
  context->math_spacing     = old_math_spacing;
  context->show_auto_styles = old_show_auto_styles;
  context->text_shaper      = old_text_shaper;
}

Expr NumberBox::to_pmath(BoxFlags flags) {
  if(has(flags, BoxFlags::ShortNumbers))
    return content()->to_pmath(flags);
    
  return _number;
}

Expr NumberBox::prepare_boxes(Expr boxes) {
  if(boxes.is_string()) {
    String s = String(boxes);
    
    const uint16_t *buf = s.buffer();
    const int       len = s.length();
    
    if(len > 0 && buf[0] >= '0' && buf[0] <= '9') {
      for(int i = 0; i < len; ++i) {
        if(buf[i] == '`' || buf[i] == '^') {
          return Call(GetSymbol(FESymbolIndex::NumberBox), s);
        }
      }
    }
    
    return s;
  }
  
  if(boxes[0] == PMATH_SYMBOL_LIST) {
    for(size_t i = 0; i <= boxes.expr_length(); ++i) {
      boxes.set(i, prepare_boxes(boxes[i]));
    }
  }
  
  return boxes;
}

//} ... class NumberBox
