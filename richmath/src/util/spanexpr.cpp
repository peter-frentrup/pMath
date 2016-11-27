#include <util/spanexpr.h>

#include <boxes/interpretationbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/underoverscriptbox.h>

using namespace richmath;

//{ class SpanExpr ...

SpanExpr::SpanExpr(int position, MathSequence *sequence)
  : Base(),
    _parent(nullptr),
    _start(position),
    _end(position - 1),
    _span(nullptr),
    _sequence(sequence)
{
}

SpanExpr::SpanExpr(int start, Span span, MathSequence *sequence)
  : Base(),
    _span(nullptr)
{
  init(nullptr, start, span, sequence);
}

SpanExpr::SpanExpr(SpanExpr *parent, int start, Span span, MathSequence *sequence)
  : Base(),
    _span(nullptr)
{
  init(parent, start, span, sequence);
}

void SpanExpr::init(SpanExpr *parent, int start, Span span, MathSequence *sequence) {
  _parent   = parent;
  _start    = start;
  _span     = span;
  _sequence = sequence;
  
  assert(_sequence);
  assert(_start >= 0 && _start < sequence->length());
  assert(!_parent || _parent->_start <= _start);
  
  sequence->ensure_spans_valid();
  
  if(_span) {
    _end = _span.end();
  }
  else {
    _end = _start;
    while(!sequence->span_array().is_token_end(_end))
      ++_end;
  }
  
  assert(_start <= _end);
  assert(_end < sequence->length());
  assert(!_parent || _parent->_end >= _end);
  
  int last_non_newline_length = 0;
  _items_pos.length(0);
  
  const uint16_t *str     = sequence->text().buffer();
  const uint16_t *str_end = str + sequence->text().length();
  
  if(str[_start] == '"' && _span && !_span.next()) {
    // strings contain no expressions
    return;
  }
  
  if(_span) {
    int prev = _start;
    int pos = prev;
    
    if(_span.next()) {
      pos = _span.next().end() + 1;
      
      if(!is_comment_start_at(str + prev, str_end)) {
        _items_pos.add(prev);
        last_non_newline_length = _items_pos.length();
      }
    }
    else {
      while(!sequence->span_array().is_token_end(pos))
        ++pos;
      ++pos;
      
      if(str[prev] > ' ') {
        _items_pos.add(prev); 
        last_non_newline_length = _items_pos.length();
      }
    }
    
    while(pos <= _end) {
      prev = pos;
      
      if(sequence->span_array()[pos]) {
        pos = sequence->span_array()[pos].end() + 1;
        
        if(!is_comment_start_at(str + prev, str_end)) {
          _items_pos.add(prev);
          last_non_newline_length = _items_pos.length();
        }
      }
      else {
        while(!sequence->span_array().is_token_end(pos))
          ++pos;
        ++pos;
        
        if(str[prev] > ' ') {
          _items_pos.add(prev);
          last_non_newline_length = _items_pos.length();
        }
        else if(str[prev] == '\n' && last_non_newline_length > 0) { 
          /* \n between spans/tokens is significant, \n at start or end of span is not. */
          _items_pos.add(prev);
        }
      }
    }
  }
  
  _items_pos.length(last_non_newline_length);
  _items.length(last_non_newline_length, 0);
}

SpanExpr::~SpanExpr() {
  if(_parent) {
    for(int i = 0; i < _parent->_items.length(); ++i)
      if(_parent->_items[i] == this) {
        _parent->_items[i] = nullptr;
        break;
      }
  }
  
  for(int i = 0; i < _items.length(); ++i)
    if(_items[i]) {
      _items[i]->_parent = nullptr;
      delete _items[i];
    }
}

SpanExpr *SpanExpr::find(MathSequence *sequence, int pos, bool before) {
  assert(sequence != nullptr);
  
  sequence->ensure_spans_valid();
  
  if(pos == sequence->length())
    before = true;
    
  int start = pos;
  if(pos > 0 && before) {
    --start;
    while(start > 0 && !sequence->span_array().is_token_end(start-1))
      --start;
  }
  
  if(start == sequence->length())
    return nullptr;
    
  Span span = sequence->span_array()[start];
  while(span && span.next() && span.next().end() >= pos - 1)
    span = span.next();
  
  if(span && !span.next()) {
    int tokend = start;
    while(tokend < span.end()) {
      if(sequence->span_array().is_token_end(tokend))
        break;
      ++tokend;
    }
    
    if(tokend < span.end() && pos <= tokend + 1)
      span = Span(0);
  }
    
  return new SpanExpr(start, span, sequence);
}

SpanExpr *SpanExpr::expand(bool self_destruction) {
  SpanExpr *result;
  
  if(_parent)
    return _parent;
    
  if(_start > _end) {
    if(!self_destruction)
      return nullptr;
      
    SpanExpr *result = new SpanExpr(_start, 0, _sequence);
    delete this;
    return result;
  }
  
  _sequence->ensure_spans_valid();
  
  int start = _start;
  Span sp = _sequence->span_array()[_start];
  while(sp && sp.next() != _span)
    sp = sp.next();
    
  if(!sp) {
  
    if(_start > 0) {
      start = _start - 1;
      while(start >= 0) {
        sp = _sequence->span_array()[start];
        if(sp && sp.end() >= _end) {
          while(sp.next() && sp.next().end() >= _end)
            sp = sp.next();
            
          break;
        }
        
        if(start == 0)
          break;
          
        --start;
      }
    }
    
    if(!sp) {
      if(self_destruction) {
        int index = 0;
        Box *box = _sequence;
        
        while(box) {
          index = box->index();
          box   = box->parent();
          
          if(MathSequence *seq = dynamic_cast<MathSequence *>(box)) {
            result = new SpanExpr(nullptr, index, nullptr, seq);
            delete this;
            return result;
          }
        }
        
        delete this;
        return nullptr;
      }
      
      return nullptr;
    }
  }
  
  result = new SpanExpr(nullptr, start, sp, _sequence);
  for(int i = 0; i < result->count(); ++i)
    if(result->item_pos(i) == _start) {
      result->_items[i] = this;
      _parent = result;
      return result;
    }
    
  if(self_destruction)
    delete this;
  return result;
}

Span SpanExpr::item_span(int i) {
  if(_items_pos[i] == _start && _span)
    return _span.next();
  else
    return _sequence->span_array()[_items_pos[i]];
}

SpanExpr *SpanExpr::item(int i) {
  if(!_items[i]) {
    _items[i] = new SpanExpr(this, _items_pos[i], item_span(i), _sequence);
  }
  
  return _items[i];
}

bool SpanExpr::equals(const char *latin1) {
  const uint16_t *str = _sequence->text().buffer();
  
  int i = 0;
  for(; latin1[i] != '\0' && _start + i <= _end; ++i)
    if(str[_start + i] != (unsigned char)latin1[i])
      return false;
      
  return _start + i == _end + 1 && latin1[i] == '\0';
}

bool SpanExpr::item_equals(int i, const char *latin1) {
  if(_items[i])
    return _items[i]->equals(latin1);
    
  const uint16_t *str = _sequence->text().buffer();
  
  int s = _items_pos[i];
  int e;
  
  if(s == _start && _span.next()) {
    e = _span.next().end();
  }
  else if(s > _start && _sequence->span_array()[s]) {
    e = _sequence->span_array()[s].end();
  }
  else {
    for(e = 0; latin1[e] != '\0' && (e == 0 || !_sequence->span_array().is_token_end(s + e - 1)); ++e)
      if(str[s + e] != (unsigned char)latin1[e])
        return false;
        
    return latin1[e] == '\0' && e > 0 && _sequence->span_array().is_token_end(s + e - 1);
  }
  
  for(i = 0; latin1[i] != '\0' && s + i <= e; ++i)
    if(str[s + i] != (unsigned char)latin1[i])
      return false;
      
  return s + i == e + 1 && latin1[i] == '\0';
}

uint16_t SpanExpr::first_char() {

  if(_start > _end)
    return 0;
    
  if(count() > 0)
    return 0;
    
  return _sequence->text()[_start];
}

String SpanExpr::as_text() {
  return _sequence->text().part(_start, length());
}

Box *SpanExpr::as_box() {
  if(!is_box())
    return nullptr;
    
  int b = 0;
  while(_sequence->item(b)->index() < _start)
    ++b;
    
  return _sequence->item(b);
}

pmath_token_t SpanExpr::as_token(int *prec) {
  if(_start > _end) {
    if(prec)
      *prec = PMATH_PREC_ANY;
    return PMATH_TOK_NONE;
  }
  
  if(count() == 0)
    return pmath_token_analyse(_sequence->text().buffer() + _start, length(), prec);
    
  if(count() == 1)
    return item(0)->as_token((prec));
    
  Box *b = as_box();
  if(b) {
    if(dynamic_cast<SubsuperscriptBox *>(b)) {
      if(prec)
        *prec = PMATH_PREC_POW;
      return PMATH_TOK_BINARY_RIGHT;
    }
    
    if( dynamic_cast<UnderoverscriptBox *>(b) ||
        dynamic_cast<StyleBox *>(b)           ||
        dynamic_cast<InterpretationBox *>(b))
    {
      MathSequence *seq = dynamic_cast<MathSequence *>(b->item(0));
      assert(seq);
      
      seq->ensure_spans_valid();
      if(seq->length() > 0) {
        SpanExpr *se = new SpanExpr(0, seq->span_array()[0], seq);
        
        pmath_token_t tok = se->as_token(prec);
        
        delete se;
        
        return tok;
      }
    }
  }
  
  if(prec)
    *prec = PMATH_PREC_ANY;
  return PMATH_TOK_NAME2;
}

int SpanExpr::as_prefix_prec(int defprec) {
  if(count() == 0) {
    int prec = pmath_token_prefix_precedence(
                 _sequence->text().buffer() + _start,
                 length(),
                 defprec);
                 
    return prec;
  }
  
  if(count() == 1)
    return item(0)->as_prefix_prec(defprec);
    
  Box *b = as_box();
  if(b) {
    if( dynamic_cast<UnderoverscriptBox *>(b) ||
        dynamic_cast<StyleBox *>(b)           ||
        dynamic_cast<InterpretationBox *>(b))
    {
      MathSequence *seq = dynamic_cast<MathSequence *>(b->item(0));
      assert(seq);
      
      seq->ensure_spans_valid();
      if(seq->length() > 0) {
        SpanExpr *se = new SpanExpr(0, seq->span_array()[0], seq);
        
        int prec = se->as_prefix_prec(defprec);
        
        delete se;
        
        return prec;
      }
    }
  }
  
  return defprec + 1;
}

int SpanExpr::precedence(int *pos) {
  int dummy_pos;
  if(!pos)
    pos = &dummy_pos;
    
  *pos = 0;
  
  if(count() == 0) {
    int prec;
    pmath_token_analyse(_sequence->text().buffer() + _start, length(), &prec);
    return prec;
  }
  
  if(count() == 1) {
    return item(0)->precedence(pos);
  }
  
  int prec;
  pmath_token_t tok = item(0)->as_token(&prec);
  switch(tok) {
    case PMATH_TOK_NONE:
    case PMATH_TOK_SPACE:
    case PMATH_TOK_DIGIT:
    case PMATH_TOK_STRING:
    case PMATH_TOK_NAME:
    case PMATH_TOK_NAME2:
    case PMATH_TOK_SLOT:
      break;
      
    case PMATH_TOK_NEWLINE:
    case PMATH_TOK_NARY_OR_PREFIX:
    case PMATH_TOK_POSTFIX_OR_PREFIX:
    case PMATH_TOK_PLUSPLUS:
      prec = item(0)->as_prefix_prec(prec);
      if(count() == 2) {
        *pos = -1;
        goto FINISH;
      }
      return prec;
      
    case PMATH_TOK_BINARY_LEFT:
    case PMATH_TOK_BINARY_RIGHT:
    case PMATH_TOK_NARY:
    case PMATH_TOK_NARY_AUTOARG:
    case PMATH_TOK_PREFIX:
    case PMATH_TOK_POSTFIX:
    case PMATH_TOK_CALL:
    case PMATH_TOK_ASSIGNTAG:
    case PMATH_TOK_COLON:
    case PMATH_TOK_TILDES:
    case PMATH_TOK_INTEGRAL:
      if(count() == 2) {
        *pos = -1;
        goto FINISH;
      }
      return prec;
      
    case PMATH_TOK_LEFTCALL:
    case PMATH_TOK_LEFT:
    case PMATH_TOK_RIGHT:
    case PMATH_TOK_COMMENTEND:
      return PMATH_PREC_PRIM;
      
    case PMATH_TOK_PRETEXT:
    case PMATH_TOK_QUESTION:
      if(count() == 2) {
        *pos = -1;
        prec = PMATH_PREC_CALL;
        goto FINISH;
      }
      return PMATH_PREC_CALL;
  }
  
  tok = item(1)->as_token(&prec);
  switch(tok) {
    case PMATH_TOK_NONE:
    case PMATH_TOK_SPACE:
    case PMATH_TOK_DIGIT:
    case PMATH_TOK_STRING:
    case PMATH_TOK_NAME:
    case PMATH_TOK_NAME2:
    case PMATH_TOK_LEFT:
    case PMATH_TOK_PREFIX:
    case PMATH_TOK_PRETEXT:
    case PMATH_TOK_TILDES:
    case PMATH_TOK_SLOT:
    case PMATH_TOK_INTEGRAL:
      prec = PMATH_PREC_MUL;
      break;
      
    case PMATH_TOK_CALL:
    case PMATH_TOK_LEFTCALL:
      prec = PMATH_PREC_CALL;
      break;
      
    case PMATH_TOK_RIGHT:
    case PMATH_TOK_COMMENTEND:
      prec = PMATH_PREC_PRIM;
      break;
      
    case PMATH_TOK_BINARY_LEFT:
    case PMATH_TOK_QUESTION:
    case PMATH_TOK_BINARY_RIGHT:
    case PMATH_TOK_ASSIGNTAG:
    case PMATH_TOK_NARY:
    case PMATH_TOK_NARY_AUTOARG:
    case PMATH_TOK_NARY_OR_PREFIX:
    case PMATH_TOK_COLON:
    case PMATH_TOK_NEWLINE:
      //prec = prec;  // TODO: what did i mean here?
      break;
      
    case PMATH_TOK_POSTFIX_OR_PREFIX:
    case PMATH_TOK_POSTFIX:
      if(count() == 2)
        *pos = +1;
      prec = prec;
      break;
      
    case PMATH_TOK_PLUSPLUS:
      if(count() == 2) {
        *pos = +1;
        prec = PMATH_PREC_INC;
      }
      else {
        prec = PMATH_PREC_STR;
      }
      break;
  }
  
FINISH:
  if(*pos >= 0) { // infix or postfix
    int prec2, pos2;
    
    prec2 = item(0)->precedence(&pos2);
    
    if(pos2 > 0 && prec2 < prec) { // starts with postfix operator, eg. box = a+b&*c = ((a+b)&)*c
      prec = prec2;
      *pos = 0;
    }
  }
  
  if(*pos <= 0) { // prefix or infix
    int prec2, pos2;
    
    prec2 = item(count() - 1)->precedence(&pos2);
    
    if(pos2 < 0 && prec2 < prec) { // ends with prefix operator, eg. box = a*!b+c = a*(!(b+c))
      prec = prec2;
      *pos = 0;
    }
  }
  
  return prec;
}

uint16_t SpanExpr::item_first_char(int i) {
  if(_items[i])
    return _items[i]->first_char();
    
  int s = _items_pos[i];
  
  if(s == _start) {
    if(_span.next())
      return 0;
  }
  else if(_sequence->span_array()[s])
    return 0;
    
  return _sequence->text()[s];
}

uint16_t SpanExpr::item_as_char(int i) {
  if(_items[i])
    return _items[i]->as_char();
  
  Span subspan = item_span(i);
  if(subspan)
    return 0;
  
  if(!_sequence->span_array().is_token_end(_items_pos[i]))
    return 0;
    
  return _sequence->text()[_items_pos[i]];
}

String SpanExpr::item_as_text(int i) {
  if(_items[i])
    return _items[i]->as_text();
    
  int s = _items_pos[i];
  
  int end;
  if(s == _start && _span.next()) {
    end = _span.next().end();
  }
  else if(s > _start && _sequence->span_array()[s]) {
    end = _sequence->span_array()[s].end();
  }
  else {
    end = s;
    while(end <= _end && !_sequence->span_array().is_token_end(end))
      ++end;
  }
  
  return _sequence->text().part(s, end + 1 - s);
}

Box *SpanExpr::item_as_box(int i) {
  if(!item_is_box(i))
    return nullptr;
    
  int b = 0;
  while(_sequence->item(b)->index() < _items_pos[i])
    ++b;
    
  return _sequence->item(b);
}

//} ... class SpanExpr

bool richmath::is_comment_start_at(const uint16_t *str, const uint16_t *buf_end) {
  if(buf_end <= str)
    return false;
  
  if(*str == '%')
    return true;
  
  if(str + 1 < buf_end && str[0] == '/' && str[1] == '*')
    return true;
  
  return false;
}

SpanExpr *richmath::span_as_name(SpanExpr *span) {
  if(!span)
    return nullptr;
  
  while(span->count() == 1)
    span = span->item(0);
  
  if(span->count() != 0)
    return nullptr;
  
  if(span->as_token() == PMATH_TOK_NAME)
    return span;
  
  return nullptr;
}

//{ class SequenceSpan ...

SequenceSpan::SequenceSpan(SpanExpr *span, bool take_ownership)
  : _span(span),
    _has_ownership (take_ownership)
{
  init(span);
}

void SequenceSpan::set(SpanExpr *span, bool take_ownership) {
  if(_span == span) {
    _has_ownership = take_ownership;
    return;
  }
  
  init(span);
  _has_ownership = take_ownership;
}

SequenceSpan &SequenceSpan::operator=(const SequenceSpan &other) {
  if(this == &other)
    return *this;
    
  init(other._span);
  _has_ownership = other._has_ownership;
  
  return *this;
}

SequenceSpan::~SequenceSpan() {
  reset();
}

void SequenceSpan::init(SpanExpr *span) {
  reset();
  
  if(!span)
    return;
    
  _span = span;
  if(span->count() < 1) {
    if(span->equals(",")) {
      _items.add(new SpanExpr(span->start(),     span->sequence()));
      _items.add(new SpanExpr(span->start() + 1, span->sequence()));
      _is_sequence = true;
      return;
    }
    
    _items.add(span);
    _is_sequence = true;
    return;
  }
  
  if(span->count() > 1) {
    if( !span->item_equals(0, ",") &&
        !span->item_equals(1, ","))
    {
      _items.add(span);
      _is_sequence = true;
      return;
    }
  }
  
  bool prev_was_operand = false;
  
  for(int i = 0; i < span->count(); ++i) {
    if(span->item_equals(i, ",")) {
      if(!prev_was_operand)
        _items.add(new SpanExpr(span->item_pos(i), span->sequence()));
        
      prev_was_operand = false;
      continue;
    }
    
    if(!span->item_is_operand(i)) {
      _items.length(0);
      return;
    }
    
    _items.add(span->item(i));
    prev_was_operand = true;
  }
  
  if(!prev_was_operand)
    _items.add(new SpanExpr(span->start(), span->sequence()));
    
  _is_sequence = true;
}

void SequenceSpan::reset() {
  for(int i = 0; i < _items.length(); ++i)
    if(_items[i] != _span && _items[i]->parent() == 0)
      delete _items[i];
      
  if(_has_ownership)
    delete _span;
    
  _items.length(0);
  _span        = 0;
  _is_sequence = false;
}

SpanExpr *SequenceSpan::item(int i) { // 1-based; always may return 0
  if(i <= 0 || i > _items.length())
    return nullptr;
    
  return _items[i - 1];
}

//} ... class SequenceSpan

//{ class FunctionCallSpan ...

FunctionCallSpan::FunctionCallSpan(SpanExpr *span)
  : _span(span)
  , _args(nullptr, false)
{
  init_args();
}

void FunctionCallSpan::init_args() {
  if(is_list()) {
    if(_span->count() <= 1) {   // {
      _args.set(new SpanExpr(_span->end() + 1, _span->sequence()), true);
      return;
    }
    
    if(!_span->item_is_operand(1)) {   // {}
      _args.set(new SpanExpr(_span->item_pos(1), _span->sequence()), true);
      return;
    }
    
    _args.set(_span->item(1), false);
    assert(_args.all()->parent() == _span);
    return;
  }
  
  if(is_simple_call()) {
    if(_span->count() <= 2) {   // f(
      _args.set(new SpanExpr(_span->end() + 1, _span->sequence()), true);
      return;
    }
    
    if(!_span->item_is_operand(2)) {   // f()
      _args.set(new SpanExpr(_span->item_pos(2), _span->sequence()), true);
      return;
    }
    
    _args.set(_span->item(2), false);
    return;
  }
  
  if(is_complex_call()) {
    if(_span->count() <= 4) {   // a.f(   or shorter
      _args.set(new SpanExpr(_span->end() + 1, _span->sequence()), true);
      return;
    }
    
    if(!_span->item_is_operand(4)) {   // a.f()
      _args.set(new SpanExpr(_span->item_pos(4), _span->sequence()), true);
      return;
    }
    
    _args.set(_span->item(4), false);
    return;
  }
}

bool FunctionCallSpan::is_simple_call(SpanExpr *span) {
  // 0123
  // f(
  // f()
  // f(a
  // f(a)
  
  if(!span)
    return false;
    
  if(span->count() < 2)
    return false;
    
  if(span->count() > 4)
    return false;
    
  if(!span->item_equals(1, "("))
    return false;
    
  if(!span->item_is_operand(0))
    return false;
    
  if(span->count() == 2)
    return true;
    
  if(span->count() == 3) {
    if(span->item_equals(2, ")"))
      return true;
      
    if(span->item_is_operand(2))
      return true;
      
    return false;
  }
  
  if(!span->item_equals(3, ")"))
    return false;
    
  if(!span->item_is_operand(2))
    return false;
    
  return true;
}

bool FunctionCallSpan::is_complex_call(SpanExpr *span) {
  // 012345
  // a.f
  // a.f(
  // a.f()
  // a.f(b
  // a.f(b)
  
  if(!span)
    return false;
    
  if(span->count() < 3)
    return false;
    
  if(span->count() > 6)
    return false;
    
  if(!span->item_equals(1, "."))
    return false;
    
  if(!span->item_is_operand(0))
    return false;
    
  if(!span->item_is_operand(2))
    return false;
    
  if(span->count() == 3)
    return true;
    
  if(!span->item_equals(3, "("))
    return false;
    
  if(span->count() == 4)
    return true;
    
  if(span->count() == 5) {
    if(span->item_equals(4, ")"))
      return true;
      
    if(span->item_is_operand(4))
      return true;
      
    return false;
  }
  
  if(!span->item_equals(5, ")"))
    return false;
    
  if(!span->item_is_operand(4))
    return false;
    
  return true;
}

bool FunctionCallSpan::is_list(SpanExpr *span) {
  // 012
  // {
  // {}
  // {a}
  
  if(!span)
    return false;
    
  if(span->count() < 1)
    return false;
    
  if(span->count() > 3)
    return false;
    
  if(!span->item_equals(0, "{"))
    return false;
    
  if(span->count() == 1)
    return true;
    
  if(span->count() == 2) {
    if(span->item_equals(1, "}"))
      return true;
      
    if(span->item_is_operand(1))
      return true;
      
    return false;
  }
  
  if(!span->item_equals(2, "}"))
    return false;
    
  if(!span->item_is_operand(1))
    return false;
    
  return true;
}

bool FunctionCallSpan::is_sequence(SpanExpr *span) {
  if(!span)
    return false;
    
  //bool prev_was_operand = false;
  
  for(int i = 0; i < span->count(); ++i) {
    if(span->item_equals(i, ",")) {
      //prev_was_operand = false;
      continue;
    }
    
    if(!span->item_is_operand(i))
      return false;
      
    //prev_was_operand = true;
  }
  
  return true;
}

SpanExpr *FunctionCallSpan::function_head() {
  if(is_simple_call())
    return _span->item(0);
    
  if(is_complex_call())
    return _span->item(2);
    
  return nullptr;
}

SpanExpr *FunctionCallSpan::function_argument(int i) {
  if(is_simple_call()) {
    if(_span->count() <= 2)   // f(
      return nullptr;
      
    if(!_span->item_is_operand(2)) // f()
      return nullptr;
      
    return _args.item(i);
  }
  
  if(is_complex_call()) {
    if(i == 1)
      return _span->item(0);
      
    if(_span->count() <= 4) // a.f(   or shorter
      return nullptr;
      
    if(!_span->item_is_operand(4)) // a.f()
      return nullptr;
      
    return _args.item(i - 1);
  }
  
  return nullptr;
}

int FunctionCallSpan::function_argument_count() {
  int i = 1;
  while(function_argument(i))
    ++i;
    
  return i - 1;
}

SpanExpr *FunctionCallSpan::list_element(int i) {
  if(is_list()) {
    if(_span->count() <= 1)   // {
      return nullptr;
      
    if(!_span->item_is_operand(1))
      return nullptr;
      
    return _args.item(i);
  }
  
  if(is_list())
    return function_argument(i);
    
  return nullptr;
}

int FunctionCallSpan::list_length() {
  int i = 1;
  while(list_element(i))
    ++i;
    
  return i - 1;
}

//} ... class FunctionCallSpan

//{ class BlockSpan ...

bool BlockSpan::maybe_block(SpanExpr *span) {
  if(!span)
    return false;
  
  if(span->count() < 2)
    return false;
  
  for(int i = 0;i < span->count();++i) {
    if(!span->item_is_operand(i))
      return false;
  }
  
  bool have_list = false;
  bool allow_list = false;
  for(int i = 0;i < span->count();++i) {
    SpanExpr *item = span->item(i);
    
    if(allow_list && FunctionCallSpan::is_list(item)) {
      have_list = true;
      allow_list = false;
      continue;
    }
    
    allow_list = true;
    if(span_as_name(item)) 
      continue;
    
    if(!FunctionCallSpan::is_simple_call(item))
      return false;
  }
  
  return have_list;
}

//} ... class BlockSpan
