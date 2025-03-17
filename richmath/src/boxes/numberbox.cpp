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

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>


namespace richmath { namespace strings {
  extern String EmptyString;
}}

extern pmath_symbol_t richmath_System_List;

using namespace richmath;

static const int DefaultMachinePrecisionDigits = 6;

namespace {
  static const uint16_t MULTIPLICATION_SPACE_CHAR = 0x2006;
  static const uint16_t TIMES_CHAR = 0x00D7;
  
//  static const uint16_t RadiusOpening[2] = { '[', PMATH_CHAR_PLUSMINUS };
//  static const int RadiusOpeningLength = 2;

  //static const uint16_t Multiplication[3] = { MULTIPLICATION_SPACE_CHAR, TIMES_CHAR, MULTIPLICATION_SPACE_CHAR };
  //static const int MultiplicationLength = 3;
  static const uint16_t Multiplication[1] = { TIMES_CHAR };
  static const int MultiplicationLength = 1;
  
  static const uint16_t DigitsPrefix[1] = { PMATH_CHAR_NOMINALDIGITS };
  static const int DigitsPrefixLength = 1;

  struct NumberPartPositions {
    String _number;
    
    Interval<int> mid_significant_range;
    int mid_decimal_dot;
    Interval<int> mid_insignificant_range;
    Interval<int> radius_mant_range;
    Interval<int> radius_exp_range;
    Interval<int> precision_range;
    Interval<int> exponent_range;
    
    explicit NumberPartPositions(String number);
    
    bool has_radix() { return mid_significant_range.from > 2; }
    int radix_end() { return (mid_significant_range.from > 2) ? mid_significant_range.from - 2 : 0; }
    String radix() { return (mid_significant_range.from > 2) ? _number.part(0, radix_end()) : String(); } // TODO: support "0x" base as "16^^"
    
    String midpoint_significant() {   return _number.part(mid_significant_range.from,   mid_significant_range.length()); }
    String midpoint_insignificant() { return _number.part(mid_insignificant_range.from, mid_insignificant_range.length()); }
    
    bool has_midpoint_insignificant() { return mid_insignificant_range.length() > 0; }
    
    bool has_radius() { return radius_mant_range.length() > 0; }
    String radius_mantissa() { return _number.part(radius_mant_range.from, radius_mant_range.length()); }
    
    bool has_radius_exponent() { return radius_exp_range.length() > 0; }
    String radius_exponent() { return _number.part(radius_exp_range.from, radius_exp_range.length()); }
    
    bool has_exponent() { return exponent_range.length() > 0; }
    String exponent() { return _number.part(exponent_range.from, exponent_range.length()); }
    
    int parse_base();
    
    static uint16_t digit_value_or_zero(uint16_t ch) {
      if(ch >= '0' && ch <= '9')
        return ch - '0';
      if(ch >= 'a' && ch <= 'z')
        return ch - 'a' + 10;
      if(ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 10;
      return 0;
    }
  };
}

namespace richmath {
  class NumberBox::Impl {
    public:
      Impl(NumberBox &self) : self(self) {}
      
      void set_number(String n);
      PositionInRange selection_to_string_index(Box *selbox, int selpos);
      Box *string_index_to_selection(int char_index, int *selection_index);
      
      void resize_check_auto_formatting_change(Context &context);
    
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
      
      MathSequence *append_radix(NumberPartPositions &parts);
      MathSequence *append_superscript(NumberPartPositions &parts, String s);
      
    private:
      NumberBox &self;
  };
}

//{ class NumberBox ...

NumberBox::NumberBox()
  : base(new MathSequence)
{
  use_auto_formatting(true);
  Impl(*this).set_number(strings::EmptyString);
}

NumberBox::NumberBox(String number)
  : base(new MathSequence)
{
  use_auto_formatting(true);
  Impl(*this).set_number(number);
}

bool NumberBox::try_load_from_object(Expr expr, BoxInputFlags opts) {
  if(!expr.item_equals(0, richmath_FE_NumberBox))
    return false;
    
  if(expr.expr_length() != 1)
    return false;
    
  String s(expr[1]);
  if(s.is_null())
    return false;
    
  Impl(*this).set_number(s);
  finish_load_from_object(PMATH_CPP_MOVE(expr));
  return true;
}

MathSequence *NumberBox::as_inline_span() {
  return dynamic_cast<MathSequence*>(content());
}

bool NumberBox::edit_selection(SelectionReference &selection, EditAction action) {
  if(!Box::edit_selection(selection, action)) 
    return false;
    
  auto seq = dynamic_cast<AbstractSequence*>(parent());
  if(!seq)
    return false;
    
  Box *selbox = selection.get();
  PositionInRange pos_start = Impl(*this).selection_to_string_index(selbox, selection.start);
  PositionInRange pos_end = Impl(*this).selection_to_string_index(selbox, selection.end);
  
  if(!pos_start.is_valid() || !pos_end.is_valid())
    return false;
  
  if(action == EditAction::DryRun)
    return true;
  
  int i = _index;
  
  seq->insert(_index + 1, _number);
  seq->remove(_index, _index + 1); // deletes this
  
  selection.set(seq, i + pos_start.pos, i + pos_end.pos);
  return true;
}

void NumberBox::paint(Context &context) {
  bool                  old_math_spacing     = context.math_spacing;
  bool                  old_show_auto_styles = context.show_auto_styles;
  SharedPtr<TextShaper> old_text_shaper      = context.text_shaper;
  
  context.math_spacing     = false;
  context.show_auto_styles = false;
  
  if(old_math_spacing)
    context.text_shaper = context.math_shaper;
    
  base::paint(context);
  
  context.math_spacing     = old_math_spacing;
  context.show_auto_styles = old_show_auto_styles;
  context.text_shaper      = old_text_shaper;
  
  if(get_style(AutoNumberFormating)) {
    if(!use_auto_formatting())
      invalidate();
  }
  else {
    if(use_auto_formatting())
      invalidate();
  }
}

void NumberBox::resize_inline(Context &context) {
  Impl(*this).resize_check_auto_formatting_change(context);
  base::resize_inline(context);
}

void NumberBox::resize_default_baseline(Context &context) {
  Impl(*this).resize_check_auto_formatting_change(context);
  base::resize_default_baseline(context);
}

Expr NumberBox::to_pmath_impl(BoxOutputFlags flags) {
  if(has(flags, BoxOutputFlags::ShortNumbers))
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
        if(buf[i] == '`' || buf[i] == '^' || buf[i] == '.') {
          pmath_t debug_metadata = pmath_get_debug_metadata(s.get());
          Expr result = Call(Symbol(richmath_FE_NumberBox), s);
          return Expr{ pmath_try_set_debug_metadata(result.release(), debug_metadata) };
        }
      }
    }
    
    return s;
  }
  
  if(boxes.item_equals(0, richmath_System_List)) {
    pmath_t old_debug_metadata = pmath_get_debug_metadata(boxes.get());
    for(size_t i = 0; i <= boxes.expr_length(); ++i) {
      boxes.set(i, prepare_boxes(boxes[i]));
    }
    return Expr{ pmath_try_set_debug_metadata(boxes.release(), old_debug_metadata) };
  }
  
  return boxes;
}

void NumberBox::dynamic_updated() {
  base::dynamic_updated(); // TODO: invalidate() instead of request_repaint_all() ?
}

bool NumberBox::is_number_part(Box *box) {
  if(!box)
    return false;
    
  return box == content() || 
         box == _base || 
         box == _radius_base || 
         box == _radius_exponent || 
         box == _exponent;
}

PositionInRange NumberBox::selection_to_string_index(String number, Box *sel, int index) {
  if(number != _number)
    return PositionInRange(-1, 0, 0);
  
  return Impl(*this).selection_to_string_index(sel, index);
}

Box *NumberBox::string_index_to_selection(String number, int char_index, int *selection_index) {
  *selection_index = -1;
  if(number != _number) {
    if( number.length() == _number.length() + 2 && 
        number[0] == '"' && 
        number.part(1, _number.length()) == _number)
    {
      return Impl(*this).string_index_to_selection(char_index - 1, selection_index);
    }
    return nullptr;
  }
  
  return Impl(*this).string_index_to_selection(char_index, selection_index);
}
      
//} ... class NumberBox

//{ class NumberBox::Impl ...

void NumberBox::Impl::set_number(String n) {
  self._number = n;
  self._base = nullptr;
  self._radius_base = nullptr;
  self._radius_exponent = nullptr;
  self._exponent = nullptr;
  
  self._content->remove(0, self._content->length());
  
  if(self.use_auto_formatting()) {
    NumberPartPositions parts{ n };
    
    int base = parts.parse_base();
    
    if(parts.precision_range.length() == 0 && parts.mid_significant_range.to + 1 >= parts.precision_range.from) {
      // There is at most a single character between midpoint significant digits and precision. 
      // That is a single backtick (`).
      // This implies !parts.has_midpoint_insignificant(), as well as !parts.has_radius()
      int num_digits = parts.mid_significant_range.length();
      if(parts.mid_significant_range.from <= parts.mid_decimal_dot && parts.mid_decimal_dot < parts.mid_significant_range.to)
        num_digits -= 1;
      
      if(DefaultMachinePrecisionDigits < num_digits) {
        int new_end = parts.mid_significant_range.to - num_digits + DefaultMachinePrecisionDigits;
        if(new_end <= parts.mid_decimal_dot + 1)
          new_end = parts.mid_decimal_dot + 2;
        
        if(base == 10) {
          const uint16_t *buf = parts._number.buffer();
          if(buf[new_end] >= (uint16_t)'5') { // round upwards. TODO: round to even instead??
            int pos_last_rounded = new_end - 1;
            while(pos_last_rounded >= parts.mid_significant_range.from && (buf[pos_last_rounded] == '9' || buf[pos_last_rounded] == '.'))
              --pos_last_rounded;
            
            if(parts.mid_significant_range.from <= pos_last_rounded) {
              parts._number.edit([&](uint16_t *new_buf, int len) {
                new_buf[pos_last_rounded] += 1;
                for(int i = pos_last_rounded + 1; i < new_end; ++i) {
                  if(new_buf[i] != '.')
                    new_buf[i] = '0';
                }
              });
            }
            else {
              // All digits are '9'. Need to set those to '0' and prepend a '1'.
              // But have an unused digit at new_end. So instead of prepending, set first digit to '1',
              // set all other up to and including new_end to '0', shift new_end and decimal dot by one place  
              parts._number.edit([&](uint16_t *new_buf, int len) {
                new_buf[parts.mid_significant_range.from] = '1';
                for(int i = parts.mid_significant_range.from + 1; i <= new_end; ++i) {
                  new_buf[i] = '0';
                }

                new_end += 1;
                parts.mid_decimal_dot+= 1;
                new_buf[parts.mid_decimal_dot] = '.';
              });
            }
          }

          parts.mid_significant_range.to = new_end;
        }
      }
    }

    if(base != 10)
      append(DigitsPrefix, DigitsPrefixLength);
    
    append(parts.midpoint_significant());
    
    if(!parts.has_radius() || !parts.has_midpoint_insignificant())
      self._base = append_radix(parts);
    
    // TODO: only show radius when requested or when it is particularly big
    if(parts.has_radius()) {
      append('[');
      
      if(parts.has_midpoint_insignificant()) {
        if(base != 10)
          append(DigitsPrefix, DigitsPrefixLength);
          
        append(parts.midpoint_insignificant());
        
        assert(self._base == nullptr);
        self._base = append_radix(parts);
      }
      
      append(PMATH_CHAR_PLUSMINUS);
      if(base != 10)
        append(DigitsPrefix, DigitsPrefixLength);
      
      append(parts.radius_mantissa());
      self._radius_base = append_radix(parts);
      
      if(parts.has_radius_exponent())
        self._radius_exponent = append_superscript(parts, parts.radius_exponent());
        
      append(']');
    }
    
    if(parts.has_exponent())
      self._exponent = append_superscript(parts, parts.exponent());
  }
  else {
    append(n);
  }
}

PositionInRange NumberBox::Impl::selection_to_string_index(Box *selbox, int selpos) {
  NumberPartPositions parts{ self._number };
  
  if(selbox == nullptr)
    return PositionInRange(-1, 0, 0); // error
    
  if(selbox == self._exponent)
    return PositionInRange::relative(parts.exponent_range, selpos);
    
  if(selbox == self._radius_exponent)
    return PositionInRange::relative(parts.radius_exp_range, selpos);
    
  if(selbox == self._base || selbox == self._radius_base)
    return PositionInRange(selpos, 0, parts.radix_end());
    
  // We should be in the midpoint significant mantissa, mitpoint insignificant mantissa, radius mantissa 
  // or one of the "["", "+/-", "]" or "*10" parts.
  if(selbox != self._content)
    return PositionInRange(-1, 0, 0); // error
    
  int len = self._content->length();
  if(selpos <= 0)
    return PositionInRange(0, 0, parts.mid_significant_range.to);
    
  if(selpos >= len)
    return PositionInRange(self._number.length(), parts.exponent_range);
    
  const uint16_t *buf = self._content->text().buffer();
  int i = selpos;
  while(i > 0 && (buf[i - 1] == '.' || pmath_char_is_36digit(buf[i - 1])))
    --i;
    
  if(i == 0)
    return PositionInRange::relative(parts.mid_significant_range, selpos);
  
  if(buf[i - 1] == PMATH_CHAR_NOMINALDIGITS) {
    // Only the three mantissa digits sequences are prefixed with \[NominalDigits]
    if(i == 1)
      return PositionInRange::relative(parts.mid_significant_range, selpos - i);
    else if(buf[i - 2] == '[')
      return PositionInRange::relative(parts.mid_insignificant_range, selpos - i);
    else
      return PositionInRange::relative(parts.radius_mant_range, selpos - i);
  }
  
  if(buf[i - 1] == PMATH_CHAR_PLUSMINUS) {
    if(i >= 2 && buf[i - 2] == '[')
      return PositionInRange::relative(parts.mid_insignificant_range, selpos - i);
    else
      return PositionInRange::relative(parts.radius_mant_range, selpos - i);
  }
    
  if(buf[i - 1] == '[') {
    return PositionInRange::relative(parts.mid_insignificant_range, selpos - i);
  }
    
  if(buf[i - 1] == MULTIPLICATION_SPACE_CHAR || buf[i - 1] == TIMES_CHAR) {
    // in one of the two "*^" exponent specifiers
    int j = selpos;
    while(j < len && 
        (buf[j] == MULTIPLICATION_SPACE_CHAR || 
        buf[j] == TIMES_CHAR || 
        buf[j] == '.' || 
        buf[j] == PMATH_CHAR_BOX || 
        pmath_char_is_36digit(buf[j])))
    {
      ++j;
    }
      
    if(j == len) {
      return PositionInRange(parts.exponent_range.from - 1, parts.precision_range.to, parts.exponent_range.from);
    }
      
    if(buf[j] == ']')
      return PositionInRange(parts.radius_mant_range.to, parts.radius_mant_range.to, parts.radius_exp_range.from);
  }
  else if(buf[i - 1] == ']' || buf[i] == ']')
      return PositionInRange(parts.exponent_range.from - 1, parts.radius_exp_range.to, parts.exponent_range.from);
  
  return PositionInRange(-1, 0, 0); // error
}

Box *NumberBox::Impl::string_index_to_selection(int char_index, int *selection_index) {
  NumberPartPositions parts{ self._number };
  
  // base^^...
  if(char_index < parts.mid_significant_range.from) {
    if(self._base) {
      *selection_index = std::max(0, std::min(char_index, self._base->length()));
      return self._base;
    }
  }
  
  const uint16_t *buf = self._content->text().buffer();
  Interval<int> buf_range {0, self._content->length()};
  
  if(char_index < parts.mid_significant_range.from) {
    if(self._base) {
      *selection_index = std::max(0, std::min(char_index, self._base->length()));
      return self._base;
    }
    
    *selection_index = 0;
    return self._content;
  }
  
  if(self._radius_exponent && parts.radius_exp_range.contains(char_index)) {
    *selection_index = std::max(0, std::min(char_index - parts.radius_exp_range.from, self._radius_exponent->length()));
    return self._radius_exponent;
  }
  
  if(self._exponent && parts.exponent_range.contains(char_index)) {
    *selection_index = std::max(0, std::min(char_index - parts.exponent_range.from, self._exponent->length()));
    return self._exponent;
  }
  
  Interval<int> content_mid_sig {0,0};
  content_mid_sig.from = 0;
  if(DigitsPrefixLength <= buf_range.length()) {
    if(0 == memcmp(buf, DigitsPrefix, sizeof(uint16_t) * DigitsPrefixLength))
      content_mid_sig.from += DigitsPrefixLength;
  }
  content_mid_sig.to = content_mid_sig.from + parts.mid_significant_range.length();
  
  if(parts.mid_significant_range.contains(char_index)) {
    *selection_index = buf_range.nearest(content_mid_sig.from + (char_index - parts.mid_significant_range.from));
    return self._content;
  }
  
  if(char_index < parts.mid_insignificant_range.from) {
    *selection_index = buf_range.nearest(content_mid_sig.to);
    return self._content;
  }
  
  Interval<int> content_mid_insig = content_mid_sig.singleton_end();
  if(content_mid_insig.from < buf_range.length() && buf[content_mid_insig.from] == '[') {
    content_mid_insig.from++;
    
    if(content_mid_insig.from + DigitsPrefixLength <= buf_range.length()) {
      if(0 == memcmp(&buf[content_mid_insig.from], DigitsPrefix, sizeof(uint16_t) * DigitsPrefixLength))
        content_mid_insig.from += DigitsPrefixLength;
    }
  }
  content_mid_insig.to = content_mid_insig.from + parts.mid_insignificant_range.length();
  
  if(parts.mid_insignificant_range.contains(char_index)) {
    *selection_index = buf_range.nearest(content_mid_insig.from + (char_index - parts.mid_insignificant_range.from));
    return self._content;
  }
  
  if(char_index < parts.radius_mant_range.from) {
    *selection_index = buf_range.nearest(content_mid_insig.to);
    return self._content;
  }
  
  Interval<int> content_rad_mant = content_mid_insig.singleton_end();
  if(content_rad_mant.from < buf_range.length() && buf[content_rad_mant.from] == PMATH_CHAR_PLUSMINUS) {
    content_rad_mant.from++;
    
    if(content_rad_mant.from + DigitsPrefixLength <= buf_range.length()) {
      if(0 == memcmp(&buf[content_rad_mant.from], DigitsPrefix, sizeof(uint16_t) * DigitsPrefixLength))
        content_rad_mant.from += DigitsPrefixLength;
    }
  }
  content_rad_mant.to = content_rad_mant.from + parts.radius_mant_range.length();
  
  if(parts.radius_mant_range.contains(char_index)) {
    *selection_index = buf_range.nearest(content_rad_mant.from + (char_index - parts.radius_mant_range.from));
    return self._content;
  }
  
  if(char_index < parts.radius_exp_range.from) { // inside the  *^ = *BASE^  of the radius exponent
    *selection_index = buf_range.nearest(content_rad_mant.to + 2);
    return self._content;
  }
  
  //assert(!parts.radius_exp_range.contains(char_index)); // checked above (if self._radius_exponent != nullptr)
  
  Interval<int> content_prec = content_rad_mant.singleton_end();
  if(self._radius_exponent) {
    content_prec.from = 1 + self._radius_exponent->index_in_ancestor(self._content, content_rad_mant.to - 1);
    content_prec.to = content_prec.from;
  }
  if(content_prec.from < buf_range.length() && buf[content_prec.from] == ']') {
    content_prec+= 1;
    if(content_prec.from < buf_range.length() && buf[content_prec.from] == '`') {
      while(content_prec.from < buf_range.length() && buf[content_prec.from] == '`') {
        content_prec.from++;
      }
      content_prec.to = content_prec.from + parts.precision_range.length();
    }
  }
    
  if(char_index < parts.precision_range.from) { // inside the ]`  or  ]``
    *selection_index = buf_range.nearest(content_prec.from - 1);
    return self._content;
  }
  
  if(parts.precision_range.contains(char_index)) {
    *selection_index = buf_range.nearest(content_prec.from + (char_index - parts.precision_range.from));
    return self._content;
  }
  
  if(char_index < parts.exponent_range.from) { // inside the  *^ = *BASE^  of the overal exponent
    *selection_index = buf_range.nearest(content_prec.to + 2);
    return self._content;
  }
  
  //assert(!parts.exponent_range.contains(char_index)); // checked above (if self._exponent != nullptr)
  
  *selection_index = buf_range.to;
  return self._content;
}

void NumberBox::Impl::resize_check_auto_formatting_change(Context &context) {
  if(self.get_style(AutoNumberFormating)) {
    if(!self.use_auto_formatting()) {
      self.use_auto_formatting(true);
      set_number(self._number);
    }
  }
  else {
    if(self.use_auto_formatting()) {
      self.use_auto_formatting(false);
      set_number(self._number);
    }
  }
}

MathSequence *NumberBox::Impl::append_radix(NumberPartPositions &parts) {
  if(!parts.has_radix())
    return nullptr;
    
  SubsuperscriptBox *bas = new SubsuperscriptBox(new MathSequence, nullptr);
  MathSequence *seq = bas->subscript();
  seq->insert(0, parts.radix());
  append(bas);
  return seq;
}

MathSequence *NumberBox::Impl::append_superscript(NumberPartPositions &parts, String s) {
  SubsuperscriptBox *exp = new SubsuperscriptBox(0, new MathSequence);
  MathSequence *seq = exp->superscript();
  seq->insert(0, s);
  
  append(Multiplication, MultiplicationLength);
  
  if(parts.has_radix())
    append(parts.radix());
  else
    append("10");
    
  append(exp);
  return exp->superscript();
}

//} ... class NumberBox::Impl

//{ class NumberPartPositions ...

NumberPartPositions::NumberPartPositions(String number)
  : mid_significant_range{0,0}
  , mid_insignificant_range{0,0}
  , radius_mant_range{0,0}
  , radius_exp_range{0,0}
  , precision_range{0,0}
  , exponent_range{0,0}
{
  _number = number;
  
  const uint16_t *buf = _number.buffer();
  const int       len = _number.length();
  
  mid_significant_range.from = 0;
  while(mid_significant_range.from < len && pmath_char_is_digit(buf[mid_significant_range.from])) 
    ++mid_significant_range.from;
  
  if( mid_significant_range.from + 1 < len        &&
      buf[mid_significant_range.from]     == '^' &&
      buf[mid_significant_range.from + 1] == '^')
  {
    mid_significant_range.from += 2;
  }
  else {
    mid_significant_range.from = 0;
  }
  
  mid_decimal_dot = -1;
  mid_significant_range.to = mid_significant_range.from;
  while(mid_significant_range.to < len) {
    if( buf[mid_significant_range.to] == '`' ||
        buf[mid_significant_range.to] == '[' ||
        buf[mid_significant_range.to] == '*'
    ) {
      break;
    }
    if(buf[mid_significant_range.to] == '.')
      mid_decimal_dot = mid_significant_range.to;
    ++mid_significant_range.to;
  }
  
  if(mid_decimal_dot < 0)
    mid_decimal_dot = mid_significant_range.to;
  
  mid_insignificant_range = mid_significant_range.singleton_end();
  if(mid_insignificant_range.from < len && buf[mid_insignificant_range.from] == '[') {
    mid_insignificant_range += 1;
    
    if(mid_insignificant_range.to < len) {
      if(buf[mid_insignificant_range.to] == PMATH_CHAR_NOMINALDIGITS || pmath_char_is_36digit(buf[mid_insignificant_range.to])) {
        do {
          ++mid_insignificant_range.to;
        } while(mid_insignificant_range.to < len && pmath_char_is_36digit(buf[mid_insignificant_range.to]));
      }
    }
    
    // skip over "+/-":
    radius_mant_range.from = mid_insignificant_range.to;
    while(radius_mant_range.from < len && !pmath_char_is_36digit(buf[radius_mant_range.from]))
      ++radius_mant_range.from;
      
    // TODO: support "p" or "e" exponent specifiers
    radius_mant_range.to = radius_mant_range.from;
    while(radius_mant_range.to < len && buf[radius_mant_range.to] != '*' && buf[radius_mant_range.to] != ']')
      ++radius_mant_range.to;
      
    radius_exp_range.from = radius_mant_range.to;
    if(radius_exp_range.from + 1 < len && buf[radius_exp_range.from] == '*' && buf[radius_exp_range.from + 1] == '^')
      radius_exp_range.from += 2;
      
    radius_exp_range.to = radius_exp_range.from;
    while(radius_exp_range.to < len && buf[radius_exp_range.to] != ']')
      ++radius_exp_range.to;
  }
  else {
    radius_mant_range       = mid_significant_range.singleton_end();
    radius_exp_range        = mid_significant_range.singleton_end();
  }
  
  precision_range.from = radius_exp_range.to;
  if(precision_range.from < len && buf[precision_range.from] == ']')
    ++precision_range.from;
  
  while(precision_range.from < len && buf[precision_range.from] == '`')
    ++precision_range.from;
    
  // TODO: support "p" or "e" exponent specifiers
  precision_range.to = precision_range.from;
  while(precision_range.to < len && buf[precision_range.to] != '*')
    ++precision_range.to;
    
  exponent_range.from = precision_range.to;
  if(exponent_range.from + 1 < len && buf[exponent_range.from] == '*' && buf[exponent_range.from + 1] == '^')
    exponent_range.from += 2;
    
  exponent_range.to = len;
}

int NumberPartPositions::parse_base() {
  int base_end = mid_significant_range.from - 2; // base^^mantissa...
  if(base_end < 0)
    return 10;
  
  int base = 0;
  
  const uint16_t *buf = _number.buffer();
  while(base_end > 0) {
    base = 10 * base + (*buf - '0');
    ++buf;
    --base_end;
    if(base > 36)
      return 36;
  }
  if(base < 2)
    return 2;
  return base;
}

//} ... class NumberPartPositions
