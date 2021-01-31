#include <syntax/autocompletion.h>

#include <boxes/mathsequence.h>
#include <boxes/inputfieldbox.h>

#include <eval/application.h>
#include <eval/binding.h>

#include <gui/document.h>

#include <syntax/spanexpr.h>

extern pmath_symbol_t richmath_FE_AutoCompleteName;
extern pmath_symbol_t richmath_FE_AutoCompleteFile;
extern pmath_symbol_t richmath_FE_AutoCompleteOther;

extern pmath_symbol_t richmath_System_ComplexStringBox;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_Keys;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_MakeBoxes;
extern pmath_symbol_t richmath_System_MakeExpression;
extern pmath_symbol_t richmath_System_String;
extern pmath_symbol_t richmath_System_Try;

using namespace richmath;

//{ class AutoCompletion::Private ...

class AutoCompletion::Private {
  public:
    Private(AutoCompletion *_pub, Document *_document);
    
    bool continue_completion(LogicalDirection direction);
    
    bool start_alias(LogicalDirection direction);
    bool start_filename(LogicalDirection direction);
    bool start_symbol(LogicalDirection direction);
    
  private:
    AutoCompletion *pub;
    
    void add_completion_if_needed(const Expr &input, LogicalDirection direction);
    
  public:
    Document *document;
    Expr current_boxes_list;
    int current_index;
};

AutoCompletion::Private::Private(AutoCompletion *_pub, Document *_document)
  : pub(_pub),
    document(_document),
    current_index(0)
{
}

bool AutoCompletion::Private::continue_completion(LogicalDirection direction) {
  if(!pub->range.id)
    return false;
    
  if(direction == LogicalDirection::Forward) {
    ++current_index;
    if((size_t)current_index > current_boxes_list.expr_length())
      current_index = 1;
  }
  else {
    --current_index;
    if(current_index < 1)
      current_index = current_boxes_list.expr_length();
  }
  
  Expr boxes = current_boxes_list[(size_t)current_index];
  
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(pub->range.get())) {
    if(!seq->get_style(Editable))
      return false;
      
    BoxInputFlags options = BoxInputFlags::Default;
    if(seq->get_style(AutoNumberFormating))
      options |= BoxInputFlags::FormatNumbers;
      
    AbstractSequence *tmp = seq->create_similar();
    tmp->load_from_object(boxes, options);
    
    seq->remove(pub->range.start, pub->range.end);
    int end = seq->insert(pub->range.start, tmp);
    
    document->move_to(seq, end);
    pub->range = SelectionReference(seq->id(), pub->range.start, end);
    
    return true;
  }
  
  return false;
}

bool AutoCompletion::Private::start_alias(LogicalDirection direction) {
  auto seq = dynamic_cast<AbstractSequence*>(document->selection_box());
  if(!seq)
    return false;
    
  int alias_end = document->selection_end();
  int alias_pos = alias_end - 1;
  
  uint32_t ch = 0;
  while(alias_pos >= 0) {
    ch = seq->char_at(alias_pos);
    
    if(ch == PMATH_CHAR_ALIASDELIMITER || (0 < ch && ch < ' '))
      break;
      
    --alias_pos;
  }
  
  if(ch != PMATH_CHAR_ALIASDELIMITER || alias_pos + 1 >= alias_end)
    return false;
    
    
  String alias = seq->raw_substring(alias_pos, alias_end - alias_pos).part(1);
  
  Expr input_aliases           = seq->get_finished_flatlist_style(InputAliases);
  Expr input_auto_replacements = seq->get_finished_flatlist_style(InputAutoReplacements);
  
  Expr expr;
  {
    Gather g;
    
    Gather::emit(Evaluate(Call(Symbol(richmath_System_Keys), input_aliases)));
    Gather::emit(Evaluate(Call(Symbol(richmath_System_Keys), input_auto_replacements)));
    
    if(alias[0] == '\\' && (alias[1] == 0 || alias[1] == '[')) {
      size_t count;
      const struct pmath_named_char_t *all_names = pmath_get_char_names(&count);
      
      String prefix = String("\\[");
      for(size_t i = 0; i < count; ++i) {
        String name = (prefix + all_names[i].name) + "]";
        
        Gather::emit(name);
      }
    }
    
    expr = g.end();
    expr = Expr{pmath_expr_flatten(expr.release(), pmath_ref(richmath_System_List), 1)};
  }
  
  expr = Application::interrupt_wait_cached(
           Call(
             Symbol(richmath_FE_AutoCompleteOther),
             expr,
             alias),
           Application::button_timeout);
           
  if(expr[0] != richmath_System_List || expr.expr_length() == 0)
    return false;
    
  {
    Gather g;
    String alias_prefix = String::FromChar(PMATH_CHAR_ALIASDELIMITER);
    alias = alias_prefix + alias;
    
    Hashset<Expr> used;
    
    for(size_t i = 1; i <= expr.expr_length(); ++i) {
      Expr item = expr[i];
      
      if(!item.is_string())
        continue;
        
      if(Expr repl = input_auto_replacements.lookup(item, {})) {
        if(used.add(repl)) 
          Gather::emit(repl);
        continue;
      }
      
      if(Expr repl = input_aliases.lookup(item, {})) {
        if(used.add(repl)) 
          Gather::emit(repl);
        continue;
      }
      
      // todo: expand \[...] characters
      
      Gather::emit(alias_prefix + item);
      
    }
    
    expr = g.end();
  }
  
  if(expr[0] != richmath_System_List || expr.expr_length() == 0)
    return false;
    
  current_boxes_list = expr;
  
  SelectionReference first_char_range;
  first_char_range.set(seq, alias_pos, alias_pos/* + 1*/); // normalizes the selection
  
  if(first_char_range.id != seq->id()) { // should not happen?
    first_char_range.set_raw(seq, alias_pos, alias_pos/* + 1*/);
  }
  
  pub->range = SelectionReference(first_char_range.id, first_char_range.end, alias_end);
  
  add_completion_if_needed(alias, direction);
  
  return true;
}

bool AutoCompletion::Private::start_filename(LogicalDirection direction) {
  auto seq = dynamic_cast<MathSequence*>(document->selection_box());
  if(!seq)
    return false;
    
  String str;
  Expr expr;
  const uint16_t *buf = seq->text().buffer();
  bool have_filename_sep = false;
  
  if(InputFieldBox *inp = dynamic_cast<InputFieldBox *>(seq->parent())) {
    if(inp->input_type == richmath_System_String) {
      if(seq->count() > 0)
        return false;
        
      int string_end = seq->length();
      if(string_end < 1)
        return false;
        
      for(int i = 0; i < document->selection_start(); ++i) {
        if( buf[i] < ' '  ||
            buf[i] == '|' ||
            buf[i] == '"' ||
            buf[i] == '<' ||
            buf[i] == '>' ||
            buf[i] == '\'' ||
            buf[i] == '*' ||
            buf[i] == '?' ||
            buf[i] == ';')
        {
          return false;
        }
        
        if(buf[i] == '/' || buf[i] == '\\')
          have_filename_sep = true;
      }
      
      if(!have_filename_sep)
        return false;
        
      for(int i = document->selection_start(); i < string_end; ++i) {
        if( buf[i] < ' '  ||
            buf[i] == '|' ||
            buf[i] == '<' ||
            buf[i] == '>' ||
            buf[i] == '/' ||
            buf[i] == '\\' ||
            buf[i] == '\'' ||
            buf[i] == '"' ||
            buf[i] == '*' ||
            buf[i] == '?' ||
            buf[i] == ';')
        {
          return false;
        }
      }
      
      str = seq->text();
      expr = Application::interrupt_wait_cached(
               Call(
                 Symbol(richmath_FE_AutoCompleteFile),
                 str),
               Application::button_timeout);
               
      if(expr[0] != richmath_System_List || expr.expr_length() == 0)
        return false;
        
      document->move_to(seq, seq->length(), false);
      pub->range = SelectionReference(seq->id(), 0, string_end);
      current_boxes_list = expr;
      
      if(str == current_boxes_list[1]) {
        if(direction == LogicalDirection::Forward)
          current_index = 1;
        else
          current_index = current_boxes_list.expr_length() + 1;
      }
      else {
        current_boxes_list.append(str);
        
        if(direction == LogicalDirection::Forward)
          current_index = 0;
        else
          current_index = current_boxes_list.expr_length();
      }
      
      return true;
    }
  }
  
  int string_end;
  int string_start = seq->find_string_start(document->selection_start(), &string_end);
  if(string_start < 0)
    return false;
    
  seq->ensure_boxes_valid();
  
  for(int i = 0; i < seq->count(); ++i) {
    if(seq->item(i)->index() >= string_end)
      break;
      
    if(seq->item(i)->index() >= string_start)
      return false;
  }
  
  
  for(int i = string_start + 1; i < document->selection_start(); ++i) {
    if( buf[i] < ' '  ||
        buf[i] == '|' ||
        buf[i] == '"' ||
        buf[i] == '<' ||
        buf[i] == '>' ||
        buf[i] == '\'' ||
        buf[i] == '*' ||
        buf[i] == '?' ||
        buf[i] == ';')
    {
      return false;
    }
    
    if(buf[i] == '\\') {
      if(i + 1 == document->selection_start() || buf[i + 1] == '"')
        return false;
        
      ++i;
      if(buf[i] == '\\')
        have_filename_sep = true;
    }
    
    if(buf[i] == '/')
      have_filename_sep = true;
  }
  
  if(!have_filename_sep)
    return false;
    
  for(int i = document->selection_start(); i < string_end; ++i) {
    if( buf[i] < ' '  ||
        buf[i] == '|' ||
        buf[i] == '<' ||
        buf[i] == '>' ||
        buf[i] == '/' ||
        buf[i] == '\'' ||
        buf[i] == '*' ||
        buf[i] == '?' ||
        buf[i] == ';')
    {
      return false;
    }
    
    if(buf[i] == '"' && i + 1 < string_end)
      return false;
      
    if(buf[i] == '\\') {
      if(i + 1 >= string_end || buf[i + 1] == '"' || buf[i + 1] == '\\')
        return false;
        
      ++i;
    }
  }
  
  str = seq->raw_substring(string_start, string_end - string_start);
  bool had_end_quote = true;
  if(str.length() < 2 || str[str.length() - 1] != '"') {
    had_end_quote = false;
    str = str + "\"";
  }
  
  expr = Call(
           Symbol(richmath_System_Try),
           Call(
             Symbol(richmath_System_MakeExpression),
             Call(Symbol(richmath_System_ComplexStringBox), str)));
  expr = Evaluate(expr);
  if(expr.expr_length() != 1 || expr[0] != richmath_System_HoldComplete)
    return false;
    
  str = String(expr[1]);
  if(!str.is_valid())
    return false;
    
  expr = Application::interrupt_wait_cached(
           Call(
             Symbol(richmath_FE_AutoCompleteFile),
             str),
           Application::button_timeout);
           
  if(expr[0] != richmath_System_List || expr.expr_length() == 0)
    return false;
    
  // enquote
  for(size_t i = expr.expr_length(); i > 0; --i) {
    String s = expr[i];
    if(!s.is_valid())
      return false;
      
    Expr boxes = Evaluate(Call(Symbol(richmath_System_MakeBoxes), s));
    if(boxes.expr_length() != 1 || boxes[0] != richmath_System_ComplexStringBox)
      return false;
      
    s = String(boxes[1]);
    if(s.length() < 2 || s[0] != '"' || s[s.length() - 1] != '"')
      return false;
      
    s = s.part(1, s.length() - 2);
    expr.set(i, s);
  }
  
  
  ++string_start;
  if(had_end_quote)
    --string_end;
    
  str = seq->raw_substring(string_start, string_end - string_start);
  
  document->move_to(seq, string_end, false);
  pub->range = SelectionReference(seq->id(), string_start, string_end);
  current_boxes_list = expr;
  
  add_completion_if_needed(str, direction);
  
  return true;
}

bool AutoCompletion::Private::start_symbol(LogicalDirection direction) {
  auto seq = dynamic_cast<MathSequence*>(document->selection_box());
  if(!seq)
    return false;
    
  SpanExpr *span = SpanExpr::find(seq, document->selection_end(), true);
  if(!span)
    return false;
    
  pmath_token_t tok = span->as_token();
  
  if(tok != PMATH_TOK_NAME) {
    delete span;
    return false;
  }
  
  String text = span->as_text();
  current_boxes_list = Application::interrupt_wait_cached(
                         Call(
                           Symbol(richmath_FE_AutoCompleteName),
                           text),
                         Application::button_timeout);
                         
  if(current_boxes_list[0] != richmath_System_List || current_boxes_list.expr_length() == 0) {
    current_boxes_list = Expr();
    delete span;
    return false;
  }
  
  document->move_to(span->sequence(), span->end() + 1, false);
  pub->range = SelectionReference(span->sequence()->id(), span->start(), span->end() + 1);
  
  //pmath_debug_print_object("[completions: ", current_boxes_list.get(), "]\n");
  
  add_completion_if_needed(text, direction);
  
  delete span;
  return true;
}

void AutoCompletion::Private::add_completion_if_needed(const Expr &input, LogicalDirection direction) {
  if(input == current_boxes_list[1]) {
    if(direction == LogicalDirection::Forward)
      current_index = 1;
    else
      current_index = current_boxes_list.expr_length() + 1;
  }
  else {
    current_boxes_list.append(input);
    
    if(direction == LogicalDirection::Forward)
      current_index = 0;
    else
      current_index = current_boxes_list.expr_length();
  }
}

//} ... class AutoCompletion::Private

//{ class AutoCompletion ...

AutoCompletion::AutoCompletion(Document *_document)
  : priv(new AutoCompletion::Private(this, _document))
{
}

AutoCompletion::~AutoCompletion() {
  delete priv;
}

bool AutoCompletion::next(LogicalDirection direction) {
  if(!range.id) {
    Box *selbox = priv->document->selection_box();
    if(!selbox || !selbox->get_style(Editable))
      return false;
      
    if(priv->start_alias(direction))
      return priv->continue_completion(direction);
      
    if(priv->start_filename(direction))
      return priv->continue_completion(direction);
      
    if(priv->start_symbol(direction))
      return priv->continue_completion(direction);
  }
  
  return priv->continue_completion(direction);
}

void AutoCompletion::stop() {
  range.reset();
}

//} ... class AutoCompletion

