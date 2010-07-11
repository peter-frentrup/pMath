#include <util/spanexpr.h>

#include <boxes/interpretationbox.h>
#include <boxes/stylebox.h>
#include <boxes/subsuperscriptbox.h>
#include <boxes/underoverscriptbox.h>

using namespace richmath;

//{ class SpanExpr ...

SpanExpr::SpanExpr(int start, Span span, MathSequence *sequence)
: Base(),
  _span(0)
{
  init(0, start, span, sequence);
}

SpanExpr::SpanExpr(SpanExpr *parent, int start, Span span, MathSequence *sequence)
: Base(),
  _span(0)
{
  init(parent, start, span, sequence);
}

void SpanExpr::init(SpanExpr *parent, int start, Span span, MathSequence *sequence){
  _parent   = parent;
  _start    = start;
  _span     = span;
  _sequence = sequence;
  
  assert(_sequence);
  assert(_start >= 0 && _start < sequence->length());
  assert(!_parent || _parent->_start <= _start);
  
  if(_span){
    _end = _span.end();
  }
  else{
    _end = _start;
    while(!sequence->span_array().is_token_end(_end)) 
      ++_end;
  }
  
  assert(_start <= _end);
  assert(_end < sequence->length());
  assert(!_parent || _parent->_end >= _end);
  
  _items_pos.length(0);
  
  const uint16_t *str = sequence->text().buffer();
  
  if(str[_start] == '"' && _span && !_span.next())
    _span = 0;
  
  if(_span){
    int prev = _start;
    int pos = prev;
    
    if(_span.next()){
      pos = _span.next().end() + 1;
      
      if(pos > prev + 1 && str[prev] != '/' && str[prev + 1] != '*')
        _items_pos.add(prev);
    }
    else{
      while(!sequence->span_array().is_token_end(pos))
        ++pos;
      ++pos;
      
      if(str[prev] > ' ')
        _items_pos.add(prev);
    }
    
    while(pos <= _end){
      prev = pos;
      
      if(sequence->span_array()[pos]){
        pos = sequence->span_array()[pos].end() + 1;
      
        if(pos > prev + 1 && (str[prev] != '/' || str[prev + 1] != '*'))
          _items_pos.add(prev);
      }
      else{
        while(!sequence->span_array().is_token_end(pos))
          ++pos;
        ++pos;
          
        if(str[prev] > ' ')
          _items_pos.add(prev);
      }
    }
  }
  
  _items.length(_items_pos.length(), 0);
}

SpanExpr::~SpanExpr(){
  if(_parent){
    for(int i = 0;i < _parent->_items.length();++i)
      if(_parent->_items[i] == this){
        _parent->_items[i] = 0;
        break;
      }
  }
  
  for(int i = 0;i < _items.length();++i)
    if(_items[i]){
      _items[i]->_parent = 0;
      delete _items[i];
    }
}

SpanExpr *find(MathSequence *sequence, int pos, bool before){
  assert(sequence != 0);
  
  if(pos == sequence->length())
    before = true;
  
  int start = pos;
  if(pos > 0 && before){
    start = pos - 1;
    while(start >= 0 && !sequence->span_array().is_token_end(start))
      --start;
    
    ++start;
  }
  
  if(start == sequence->length())
    return 0;
  
  Span span = sequence->span_array()[start];
  while(span && span.next() && span.next().end() >= pos - 1)
    span = span.next();
  
  return new SpanExpr(start, span, sequence);
}

SpanExpr *SpanExpr::expand(bool self_destruction){
  if(_parent)
    return _parent;
  
  int start = _start;
  Span sp = _sequence->span_array()[_start];
  while(sp && sp.next() != _span)
    sp = sp.next();
  
  if(!sp){
    if(_start == 0){
      if(self_destruction)
        return this;
      
      return 0;
    }
      
    start = _start - 1;
    while(start > 0){
      sp = _sequence->span_array()[start];
      while(sp && sp.next() && sp.next().end() >= _end)
        sp = sp.next();
      
      if(sp)
        break;
    }
  }
  
  SpanExpr *result = new SpanExpr(0, start, sp, _sequence);
  for(int i = 0;i < result->count();++i)
    if(result->item_pos(i) == _start){
      result->_items[i] = this;
      _parent = result;
      return result;
    }
  
  if(self_destruction)
    delete this;
  return result;
}

SpanExpr *SpanExpr::item(int i){
  if(!_items[i]){
    Span subspan(0);
    
    if(_items_pos[i] == _start && _span)
      subspan = _span.next();
    else
      subspan = _sequence->span_array()[_items_pos[i]];
      
    _items[i] = new SpanExpr(this, _items_pos[i], subspan, _sequence);
  }
  
  return _items[i];
}

bool SpanExpr::equals(const char *latin1){
  const uint16_t *str = _sequence->text().buffer();
  
  int i = 0;
  for(;latin1[i] != '\0' && _start + i <= _end;++i)
    if(str[_start + i] != (unsigned char)latin1[i])
      return false;
  
  return _start + i == _end + 1 && latin1[i] == '\0';
}

bool SpanExpr::item_equals(int i, const char *latin1){
  if(_items[i])
    return _items[i]->equals(latin1);
  
  const uint16_t *str = _sequence->text().buffer();
  
  int s = _items_pos[i];
  int e;
  
  if(s == _start && _span.next()){
    e = _span.next().end();
  }
  else if(s > _start && _sequence->span_array()[s]){
    e = _sequence->span_array()[s].end();
  }
  else{
    for(e = 0;latin1[e] != '\0' && (e == 0 || !_sequence->span_array().is_token_end(s + e - 1));++e)
      if(str[s + e] != (unsigned char)latin1[e])
        return false;
    
    return latin1[e] == '\0' && e > 0 && _sequence->span_array().is_token_end(s + e - 1);
  }
  
  for(i = 0;latin1[i] != '\0' && s + i <= e;++i)
    if(str[s + i] != (unsigned char)latin1[i])
      return false;
  
  return s + i == e + 1 && latin1[i] == '\0';
}

String SpanExpr::as_text(){
  return _sequence->text().part(_start, length());
}

Box *SpanExpr::as_box(){
  if(!is_box())
    return 0;
  
  int b = 0;
  while(_sequence->item(b)->index() < _start)
    ++b;
  
  return _sequence->item(b);
}

pmath_token_t SpanExpr::as_token(int *prec){
  if(count() == 0)
    return pmath_token_analyse(_sequence->text().buffer() + _start, length(), prec);
  
  if(count() == 1)
    return item(0)->as_token((prec));
  
  Box *b = as_box();
  if(b){
    if(dynamic_cast<SubsuperscriptBox*>(b)){
      if(prec) *prec = PMATH_PREC_POW;
      return PMATH_TOK_BINARY_RIGHT;
    }
    
    if(dynamic_cast<UnderoverscriptBox*>(b)
    || dynamic_cast<StyleBox*>(b)
    || dynamic_cast<TagBox*>(b)
    || dynamic_cast<InterpretationBox*>(b)){
      MathSequence *seq = dynamic_cast<MathSequence*>(b->item(0));
      assert(seq);
      
      seq->ensure_spans_valid();
      if(seq->length() > 0){
        SpanExpr *se = new SpanExpr(0, seq->span_array()[0], seq);
        
        pmath_token_t tok = se->as_token(prec);
        
        delete se;
        
        return tok;
      }
    }
  }
  
  if(prec) *prec = PMATH_PREC_ANY;
  return PMATH_TOK_NAME2;
}

int SpanExpr::as_prefix_prec(int defprec){
  if(count() == 0){
    int prec = pmath_token_prefix_precedence(
      _sequence->text().buffer() + _start, 
      length(),
      defprec);
    
    return prec;
  }
  
  if(count() == 1)
    return item(0)->as_prefix_prec(defprec);
  
  Box *b = as_box();
  if(b){
    if(dynamic_cast<UnderoverscriptBox*>(b)
    || dynamic_cast<StyleBox*>(b)
    || dynamic_cast<TagBox*>(b)
    || dynamic_cast<InterpretationBox*>(b)){
      MathSequence *seq = dynamic_cast<MathSequence*>(b->item(0));
      assert(seq);
      
      seq->ensure_spans_valid();
      if(seq->length() > 0){
        SpanExpr *se = new SpanExpr(0, seq->span_array()[0], seq);
        
        int prec = se->as_prefix_prec(defprec);
        
        delete se;
        
        return prec;
      }
    }
  }
  
  return defprec+1;
}

int SpanExpr::precedence(int *pos){
  int dummy_pos;
  if(!pos)
    pos = &dummy_pos;
  
  *pos = 0;
  
  if(count() == 0){
    int prec;
    pmath_token_analyse(_sequence->text().buffer() + _start, length(), &prec);
    return prec;
  }
  
  if(count() == 1){
    return item(0)->precedence(pos);
  }
  
  int prec;
  pmath_token_t tok = item(0)->as_token(&prec);
  switch(tok){
    case PMATH_TOK_NONE:
    case PMATH_TOK_SPACE:
    case PMATH_TOK_DIGIT:
    case PMATH_TOK_STRING:
    case PMATH_TOK_NAME:
    case PMATH_TOK_NAME2:
    case PMATH_TOK_SLOT:
      break;
    
    case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
    case PMATH_TOK_NARY_OR_PREFIX:
    case PMATH_TOK_POSTFIX_OR_PREFIX:
    case PMATH_TOK_PLUSPLUS:
      prec = item(0)->as_prefix_prec(prec);
      if(count() == 2){
        *pos = -1;
        goto FINISH;
      }
      return prec;
      
    case PMATH_TOK_BINARY_LEFT:
    case PMATH_TOK_BINARY_RIGHT:
    case PMATH_TOK_BINARY_LEFT_AUTOARG:
    case PMATH_TOK_NARY:
    case PMATH_TOK_NARY_AUTOARG:
    case PMATH_TOK_PREFIX:
    case PMATH_TOK_POSTFIX:
    case PMATH_TOK_CALL:
    case PMATH_TOK_ASSIGNTAG:
    case PMATH_TOK_COLON:
    case PMATH_TOK_TILDES:
    case PMATH_TOK_INTEGRAL:
      if(count() == 2){
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
      if(count() == 2){
        *pos = -1;
        prec = PMATH_PREC_CALL;
        goto FINISH;
      }
      return PMATH_PREC_CALL;
  }
  
  tok = item(1)->as_token(&prec);
  switch(tok){
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
    case PMATH_TOK_BINARY_LEFT_AUTOARG:
    case PMATH_TOK_BINARY_LEFT_OR_PREFIX:
    case PMATH_TOK_QUESTION:
    case PMATH_TOK_BINARY_RIGHT:
    case PMATH_TOK_ASSIGNTAG:
    case PMATH_TOK_NARY:
    case PMATH_TOK_NARY_AUTOARG:
    case PMATH_TOK_NARY_OR_PREFIX:
    case PMATH_TOK_COLON:
      //prec = prec;  // TODO: what did i mean here?
      break;
    
    case PMATH_TOK_POSTFIX_OR_PREFIX:
    case PMATH_TOK_POSTFIX:
      if(count() == 2) 
        *pos = +1;
      prec = prec;
      break;
      
    case PMATH_TOK_PLUSPLUS:
      if(count() == 2){
        *pos = +1;
        prec = PMATH_PREC_INC;
      }
      else{
        prec = PMATH_PREC_STR;
      }
      break;
  }
  
 FINISH:
  if(*pos >= 0){ // infix or postfix
    int prec2, pos2;
    
    prec2 = item(0)->precedence(&pos2);
    
    if(pos2 > 0 && prec2 < prec){ // starts with postfix operator, eg. box = a+b&*c = ((a+b)&)*c
      prec = prec2;
      *pos = 0;
    }
  }
  
  if(*pos <= 0){ // prefix or infix
    int prec2, pos2;
    
    prec2 = item(count()-1)->precedence(&pos2);
    
    if(pos2 < 0 && prec2 < prec){ // ends with prefix operator, eg. box = a*!b+c = a*(!(b+c))
      prec = prec2;
      *pos = 0;
    }
  }
  
  return prec;
}

uint16_t SpanExpr::item_first_char(int i){
  if(_items[i])
    return _items[i]->first_char();
  
  int s = _items_pos[i];
  
  if(s == _start){
    if(_span.next())
      return 0;
  }
  else if(_sequence->span_array()[s])
    return 0;
  
  return _sequence->text()[s];
}

uint16_t SpanExpr::item_as_char(int i){
  if(_items[i])
    return _items[i]->as_char();
  
  if(!_sequence->span_array().is_token_end(_items_pos[i]))
    return 0;
  
  return _sequence->text()[_items_pos[i]];
}

String SpanExpr::item_as_text(int i){
  if(_items[i])
    return _items[i]->as_text();
  
  int s = _items_pos[i];
  
  int end;
  if(s == _start && _span.next()){
    end = _span.next().end();
  }
  else if(s > _start && _sequence->span_array()[s]){
    end = _sequence->span_array()[s].end();
  }
  else{
    end = s;
    while(end <= _end && !_sequence->span_array().is_token_end(end))
      ++end;
  }
  
  return _sequence->text().part(s, end + 1 - s);
}

Box *SpanExpr::item_as_box(int i){
  if(!item_is_box(i))
    return 0;
  
  int b = 0;
  while(_sequence->item(b)->index() < _items_pos[i])
    ++b;
  
  return _sequence->item(b);
}

//} ... class SpanExpr
