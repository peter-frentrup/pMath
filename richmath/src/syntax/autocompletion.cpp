#include <syntax/autocompletion.h>

#include <boxes/mathsequence.h>
#include <boxes/inputfieldbox.h>

#include <eval/application.h>
#include <eval/binding.h>
#include <eval/current-value.h>
#include <eval/observable.h>

#include <gui/document.h>
#include <gui/menus.h>
#include <gui/native-widget.h>

#include <syntax/spanexpr.h>

#include <util/autovaluereset.h>


extern pmath_symbol_t richmath_FE_AttachAutoCompletionPopup;
extern pmath_symbol_t richmath_FE_AutoCompleteName;
extern pmath_symbol_t richmath_FE_AutoCompleteFile;
extern pmath_symbol_t richmath_FE_AutoCompleteOther;
extern pmath_symbol_t richmath_FE_Private_MakeEscapedStringContent;
extern pmath_symbol_t richmath_FE_Private_ParseStringContent;

extern pmath_symbol_t richmath_System_DollarFailed;
extern pmath_symbol_t richmath_System_Function;
extern pmath_symbol_t richmath_System_HoldComplete;
extern pmath_symbol_t richmath_System_Join;
extern pmath_symbol_t richmath_System_Keys;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_MakeBoxes;
extern pmath_symbol_t richmath_System_MakeExpression;
extern pmath_symbol_t richmath_System_Map;
extern pmath_symbol_t richmath_System_Options;
extern pmath_symbol_t richmath_System_PureArgument;
extern pmath_symbol_t richmath_System_StringBox;
extern pmath_symbol_t richmath_System_Try;

using namespace richmath;


namespace richmath {
  namespace strings {
    extern String AutoCompletionSuggestionIndex;
    extern String AutoCompletionSuggestionsList;
    extern String CompleteSelection;
  }
}

namespace {
  enum class CompletionStyle : uint8_t {
    InlineCycle,
    SuggestionsPopup,
  };
  
  enum class CompletionLocation {
    Symbol,
    FullStringOnly,
    InsideString,
  };
}

//{ class AutoCompletion::Private ...

class AutoCompletion::Private {
  public:
    Private(AutoCompletion *_pub, Document *_document);
    ~Private();
    
    bool continue_completion(LogicalDirection direction);
    bool set_current_completion_text() { return set_current_completion_text(current_index); }
    bool set_current_completion_text(int index);
    
    static bool has_only_valid_path_characters(ArrayView<const uint16_t> buf);
    static bool has_filename_sep(ArrayView<const uint16_t> buf);
    
    String try_start(bool allow_empty);
    String try_start_alias();
    String try_start_inputfield();
    String try_start_string();
    String try_start_symbol(bool allow_empty);
    bool start(CompletionStyle style, LogicalDirection direction);
    
    bool reapply_filter();
    
    bool attach_popup();
    void remove_popup();
    
    void add_completion_if_needed(const Expr &input, LogicalDirection direction);
    
    static Expr search_semantic_completion(SpanExpr *&span, VolatileLocation pos, CompletionLocation kind);
    
  private:
    AutoCompletion *pub;
    
  public:
    Document *document;
    FrontEndReference popup_id;
    Expr current_filter_function;
    ObservableValue<Expr> current_boxes_list;
    ObservableValue<int> current_index;
    bool no_stop_while_editing;
  
  private:
    static bool did_first_init;
    static void static_init();
    
    static bool cmd_CompleteSelection(Expr);
    static MenuCommandStatus test_CompleteSelection(Expr);
    static Expr get_AutoCompletionSuggestionIndex(FrontEndObject *obj, Expr item);
    static bool put_AutoCompletionSuggestionIndex(FrontEndObject *obj, Expr item, Expr rhs);
    static Expr get_AutoCompletionSuggestionsList(FrontEndObject *obj, Expr item);
};

bool AutoCompletion::Private::did_first_init = false;

AutoCompletion::Private::Private(AutoCompletion *_pub, Document *_document)
  : pub(_pub),
    document(_document),
    popup_id(FrontEndReference::None),
    current_index(0),
    no_stop_while_editing(false)
{
  if(!did_first_init) {
    static_init();
    did_first_init = true;
  }
}

AutoCompletion::Private::~Private() {
  remove_popup();
}

void AutoCompletion::Private::static_init() {
  Menus::register_command(strings::CompleteSelection, cmd_CompleteSelection, test_CompleteSelection);
  CurrentValue::register_provider(strings::AutoCompletionSuggestionIndex, get_AutoCompletionSuggestionIndex, put_AutoCompletionSuggestionIndex);
  CurrentValue::register_provider(strings::AutoCompletionSuggestionsList, get_AutoCompletionSuggestionsList);
}

bool AutoCompletion::Private::cmd_CompleteSelection(Expr) {
  Document *doc = Menus::current_document();
  
  if(!doc)
    return false;
  
  AutoCompletion &ac = doc->private_auto_completion(AutoCompletion::AccessToken{});
  ac.stop();
  return ac.priv->start(CompletionStyle::SuggestionsPopup, LogicalDirection::Forward);
}

MenuCommandStatus AutoCompletion::Private::test_CompleteSelection(Expr) {
  Document *doc = Menus::current_document();
  Box *selbox = doc ? doc->selection_box() : nullptr;
  
  if(!selbox || !selbox->editable())
    return MenuCommandStatus(false);
  
  return MenuCommandStatus(true);
}

Expr AutoCompletion::Private::get_AutoCompletionSuggestionIndex(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
  
  AutoCompletion &ac = doc->private_auto_completion(AutoCompletion::AccessToken{});
  
  return ac.priv->current_index.get();
}

bool AutoCompletion::Private::put_AutoCompletionSuggestionIndex(FrontEndObject *obj, Expr item, Expr rhs) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return false;
  
  if(item.item_equals(0, richmath_System_List))
    return false;
  
  if(!rhs.is_int32())
    return false;
  
  int new_index = PMATH_AS_INT32(rhs.get());
  
  AutoCompletion &ac = doc->private_auto_completion(AutoCompletion::AccessToken{});
  if(!ac.range)
    return false;
  
  if(ac.priv->current_index.unobserved_equals(new_index))
    return true;
  
  //return ac.priv->set_current_completion_text(new_index);
  ac.priv->current_index = new_index;
  return true;
}

Expr AutoCompletion::Private::get_AutoCompletionSuggestionsList(FrontEndObject *obj, Expr item) {
  Box      *box = dynamic_cast<Box*>(obj);
  Document *doc = box ? box->find_parent<Document>(true) : nullptr;
  if(!doc)
    return Symbol(richmath_System_DollarFailed);
  
  AutoCompletion &ac = doc->private_auto_completion(AutoCompletion::AccessToken{});
  
  Expr result = ac.priv->current_boxes_list;
  if(!result)
    return List();
  
  return result;
}

bool AutoCompletion::Private::continue_completion(LogicalDirection direction) {
  if(!pub->range.id)
    return false;
  
  Expr suggestions = current_boxes_list.get();
  
  int idx = current_index;
  if(direction == LogicalDirection::Forward) {
    ++idx;
    if((size_t)idx > suggestions.expr_length())
      idx = 1;
  }
  else {
    --idx;
    if(idx < 1)
      idx = suggestions.expr_length();
  }
  
  current_index = idx;
  return true;
}

bool AutoCompletion::Private::set_current_completion_text(int index) {
  if(AbstractSequence *seq = dynamic_cast<AbstractSequence *>(pub->range.get())) {
    if(!seq->editable())
      return false;
    
    Expr suggestions = current_boxes_list.get();
    if(index <= 0 || (size_t)index > suggestions.expr_length())
      return false;
    
    current_index = index;
    Expr boxes = suggestions[(size_t)index];
    
    AutoValueReset<bool> continue_editing(no_stop_while_editing);
    no_stop_while_editing = true;
    
    BoxInputFlags options = BoxInputFlags::Default;
    if(seq->get_style(AutoNumberFormating))
      options |= BoxInputFlags::FormatNumbers;
      
    AbstractSequence *tmp = seq->create_similar();
    tmp->load_from_object(boxes, options);
    
    seq->remove(pub->range.start, pub->range.end);
    int end = seq->insert(pub->range.start, tmp);
    
    pub->range = SelectionReference(seq->id(), pub->range.start, end);
    document->move_to(seq, end);
    if(Document *popup = FrontEndObject::find_cast<Document>(popup_id)) {
      popup->native()->source_range(pub->range);
      // TODO: also re-adjust `anchor` in document->_attached_popup_windows[...].
    }
    
    return true;
  }
  
  return false;
}

String AutoCompletion::Private::try_start(bool allow_empty) {
  String res;
  
  res = try_start_alias();
  if(res) return res;
  
  res = try_start_inputfield();
  if(res) return res;
  
  res = try_start_string();
  if(res) return res;
  
  return try_start_symbol(allow_empty);
}

String AutoCompletion::Private::try_start_alias() {
  auto seq = dynamic_cast<AbstractSequence*>(document->selection_box());
  if(!seq)
    return {};
    
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
    return {};
    
    
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
           
  if(!expr.item_equals(0, richmath_System_List) || expr.expr_length() == 0)
    return {};
    
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
  
  if(!expr.item_equals(0, richmath_System_List) || expr.expr_length() == 0)
    return {};
    
  current_boxes_list = expr;
  
  SelectionReference first_char_range;
  first_char_range.set(seq, alias_pos, alias_pos/* + 1*/); // normalizes the selection
  
  if(first_char_range.id != seq->id()) { // should not happen?
    first_char_range.set_raw(seq, alias_pos, alias_pos/* + 1*/);
  }
  
  pub->range = SelectionReference(first_char_range.id, first_char_range.end, alias_end);
  
  return alias;
}

String AutoCompletion::Private::try_start_inputfield() {
  auto seq = dynamic_cast<MathSequence*>(document->selection_box());
  if(!seq)
    return {};
  
  if(InputFieldBox *inp = dynamic_cast<InputFieldBox *>(seq->parent())) {
    if(inp->input_type() == InputFieldType::String) {
      if(seq->count() > 0)
        return {};
      
      int string_end = seq->length();
      if(string_end < 1)
        return {};
      
      int sel_pos = document->selection_start();
      
      ArrayView<const uint16_t> buf = buffer_view(seq->text());
      ArrayView<const uint16_t> until_sel = buf.take(sel_pos);
      
      if(!has_only_valid_path_characters(until_sel))
        return {};
      
      if(!has_filename_sep(until_sel))
        return {};
      
//      ArrayView<const uint16_t> after_sel = buf.drop(sel_pos);
//      if(!has_only_valid_path_characters(after_sel) || has_filename_sep(after_sel))
//        return {};
      
      //string_end = sel_end; // seq->length();
      String str = seq->text().part(0, string_end);
      current_filter_function = Symbol(richmath_FE_AutoCompleteFile);
      Expr expr = Application::interrupt_wait_cached(
                    Call(current_filter_function, str),
                    Application::button_timeout);
               
      if(!expr.item_equals(0, richmath_System_List) || expr.expr_length() == 0)
        return {};
        
      document->move_to(seq, seq->length(), false);
      pub->range = SelectionReference(seq->id(), 0, string_end);
      current_boxes_list = expr;
      
      return str;
    }
  }
  
  return {};
}

String AutoCompletion::Private::try_start_string() {
  auto seq = dynamic_cast<MathSequence*>(document->selection_box());
  if(!seq)
    return {};
    
  int sel_pos = document->selection_start();
  int sel_end = document->selection_end();
  
  int string_end;
  int string_start = seq->find_string_start(sel_pos, &string_end);
  if(string_start < 0)
    return {};
    
  ArrayView<const uint16_t> buf = buffer_view(seq->text());
  ArrayView<const uint16_t> until_sel = buf.take(sel_pos).drop(string_start);
  ArrayView<const uint16_t> after_sel = buf.take(string_end).drop(sel_pos);
  
  for(int i = 0; i < after_sel.length(); ++i) {
    if(after_sel[i] == '\n') {
      string_end = sel_end;
      after_sel = buf.take(string_end).drop(sel_pos);
    }
  }
  
  seq->ensure_boxes_valid();
  
  for(int i = 0; i < seq->count(); ++i) {
    if(seq->item(i)->index() >= string_end)
      break;
      
    if(seq->item(i)->index() >= string_start)
      return {};
  }
  
  
  Expr new_filter_function{};
  
  bool skip_quotes = false;
  SpanExpr *span = SpanExpr::find(seq, string_start - 1, LogicalDirection::Forward);
  if(span) {
    skip_quotes = true;
    new_filter_function = search_semantic_completion(
                            span,
                            VolatileLocation(seq, string_start), 
                            skip_quotes ? CompletionLocation::InsideString : CompletionLocation::FullStringOnly);
  }
  
  if(span) delete span;
  
  if(!new_filter_function && has_filename_sep(until_sel) && has_only_valid_path_characters(until_sel.drop(1))) {
    // until_sel[0] is the opening quote, ignored in check above
    
    new_filter_function = Symbol(richmath_FE_AutoCompleteFile);
    
    new_filter_function = Call(Symbol(richmath_System_Function), 
                            Call(Symbol(richmath_System_Map), 
                              Call(PMATH_CPP_MOVE(new_filter_function), 
                                Call(Symbol(richmath_FE_Private_ParseStringContent), 
                                  Call(Symbol(richmath_System_PureArgument), Number(1)))),
                              Symbol(richmath_FE_Private_MakeEscapedStringContent)));
  
    skip_quotes = true; // TODO: skip not only the quotes but also the whole directory prefix ...
  }
  
  if(!new_filter_function)
    return {};
  
  if(skip_quotes) {
    if(after_sel.length() > 0 && after_sel[after_sel.length() - 1] == '"') {
      string_end--;
    }
    ++string_start;
  }
  
  String text = seq->raw_substring(string_start, string_end - string_start);
  
  current_filter_function = new_filter_function;
  Expr suggestions = Application::interrupt_wait_cached(
                       Call(current_filter_function, text),
                       Application::button_timeout);
  
  if(!suggestions.item_equals(0, richmath_System_List) || suggestions.expr_length() == 0) {
    current_boxes_list = Expr();
    if(sel_pos < string_end) {
      text = seq->raw_substring(string_start, sel_pos - string_start);
      suggestions = Application::interrupt_wait_cached(
                           Call(current_filter_function, text),
                           Application::button_timeout);
      
      if(!suggestions.item_equals(0, richmath_System_List) || suggestions.expr_length() == 0)
        return {};
    }
    else 
      return {};
  }
  current_boxes_list = suggestions;
  
  //document->move_to(seq, string_end, false); // TODO: that is bad for skip_quotes == true  or if the closing quote is still missing
  pub->range = SelectionReference(seq->id(), string_start, /*string_end*/sel_pos);
  
  return text;
}

String AutoCompletion::Private::try_start_symbol(bool allow_empty) {
  auto seq = dynamic_cast<MathSequence*>(document->selection_box());
  if(!seq)
    return {};
  
  VolatileSelection tok_range{document->selection_now().end_only()};
  String text;
  SpanExpr *span = SpanExpr::find(seq, document->selection_end(), LogicalDirection::Backward);
  if(span) {
    pmath_token_t tok = span->as_token();
  
    if(tok == PMATH_TOK_NAME) {
      text      = span->as_text();
      tok_range = span->range();
    }
    else if(allow_empty && tok != PMATH_TOK_DIGIT && tok != PMATH_TOK_STRING) {
      text = "";
    }
  }
  else if(allow_empty) {
    text = "";
  }
  
  if(!text || !tok_range) {
    if(span) delete span;
    return {};
  }
  
  Expr new_filter_function{};
  
  // Grab context: active symbol definitions, current values, options names for current call...
  //       ... similar to ScopeColorizer, but backwards ...
  if(span)
    new_filter_function = search_semantic_completion(span, tok_range.start_only(), CompletionLocation::Symbol);
  
  if(span) delete span;
  
  if(!new_filter_function)
    new_filter_function = Symbol(richmath_FE_AutoCompleteName);
  
  current_filter_function = new_filter_function;
  Expr suggestions = Application::interrupt_wait_cached(
                       Call(current_filter_function, text),
                       Application::button_timeout);
                         
  if(!suggestions.item_equals(0, richmath_System_List) || suggestions.expr_length() == 0) {
    current_boxes_list = Expr();
    return {};
  }
  
  current_boxes_list = suggestions;
  
  document->move_to(tok_range.box, tok_range.end, false);
  pub->range = SelectionReference(tok_range);
  
  //pmath_debug_print_object("[completions: ", current_boxes_list.get(), "]\n");
  
  return text;
}

bool AutoCompletion::Private::start(CompletionStyle style, LogicalDirection direction) {
  if(String cur_text = try_start(style == CompletionStyle::SuggestionsPopup)) {
    if(style == CompletionStyle::SuggestionsPopup) {
      if(current_boxes_list.get().expr_length() == 1) {
        if(set_current_completion_text(1)) {
          pub->stop();
          return true;
        }
      }
      
      if(attach_popup()) {
        //current_index = 0; // continue_completion() will increase the index to 1
        //if(continue_completion(direction))
        //  return set_current_completion_text(current_index);
        current_index = 1;
        return true;
      }
    }
    
    add_completion_if_needed(cur_text, direction);
    if(continue_completion(direction))
      return set_current_completion_text();
  }
  return false;
}

bool AutoCompletion::Private::reapply_filter() {
  VolatileSelection sel = pub->range.get_all();
  AbstractSequence *seq = dynamic_cast<AbstractSequence*>(sel.box);
  if(!seq)
    return false;
  
  String text = seq->raw_substring(sel.start, sel.length());
  Expr new_suggestions;
  if(current_filter_function) {
    new_suggestions = Application::interrupt_wait_cached(
                        Call(current_filter_function, text), 
                        Application::button_timeout);
  }
  
  if(!new_suggestions.item_equals(0, richmath_System_List) || new_suggestions.expr_length() == 0)
    return false;

  current_boxes_list = new_suggestions;
  current_index = 1;
  
  return true;
}

bool AutoCompletion::Private::attach_popup() {
  if(!pub->range)
    return false;
  
  Expr popup_id_expr = Application::interrupt_wait(
                         Call(
                           Symbol(richmath_FE_AttachAutoCompletionPopup), 
                           pub->range.to_pmath()), 
                         Application::button_timeout);
  if(FrontEndReference new_popup_id = FrontEndReference::from_pmath(popup_id_expr)) {
    if(new_popup_id != popup_id)
      remove_popup();
    
    if(Document *popup_doc = FrontEndObject::find_cast<Document>(new_popup_id)) {
      if(popup_doc->native()->owner_document() == document) {
        popup_id = new_popup_id;
        return true;
      }
    }
  }
  
  remove_popup();  
  return false;
}

void AutoCompletion::Private::add_completion_if_needed(const Expr &input, LogicalDirection direction) {
  Expr suggestions = current_boxes_list.get();
  
  if(input == suggestions[1]) {
    if(direction == LogicalDirection::Forward)
      current_index = 1;
    else
      current_index = suggestions.expr_length() + 1;
  }
  else {
    suggestions.append(input);
    current_boxes_list = suggestions;
    
    if(direction == LogicalDirection::Forward)
      current_index = 0;
    else
      current_index = suggestions.expr_length();
  }
}

void AutoCompletion::Private::remove_popup() {
  if(popup_id) {
    if(Document *doc = FrontEndObject::find_cast<Document>(popup_id)) {
      doc->style.set(ClosingAction, ClosingActionDelete);
      doc->native()->close();
    }
    popup_id = FrontEndReference::None;
  }
}

bool AutoCompletion::Private::has_only_valid_path_characters(ArrayView<const uint16_t> buf) {
  for(uint16_t ch : buf) {
    if( ch < ' '  ||
        ch == '|' ||
        ch == '<' ||
        ch == '>' ||
        ch == '\'' ||
        ch == '"' ||
        ch == '*' ||
        ch == '?' ||
        ch == ';')
    {
      return false;
    }
  }
  
  return true;
}

bool AutoCompletion::Private::has_filename_sep(ArrayView<const uint16_t> buf) {
  for(uint16_t ch : buf) {
    if(ch == '\\' || ch == '/')
      return true;
  }
  
  return false;
}

/* static */
Expr AutoCompletion::Private::search_semantic_completion(SpanExpr *&span, VolatileLocation pos, CompletionLocation kind) {
  for(span = span->expand(true); span; span = span->expand(true)) {
    if(FunctionCallSpan::is_call(span)) {
      //if(FunctionCallSpan::is_list(span))
      //  continue;
      
      FunctionCallSpan func(span);
      
      int num_args_before = func.function_argument_count();
      for(; num_args_before > 0; --num_args_before) {
        if(SpanExpr *arg = func.function_argument(num_args_before)) {
          if(arg->sequence() == pos.box && arg->end() + 1 < pos.index) {
            break;
          }
        }
      }
      
      if(SpanExpr *head_name = span_as_name(func.function_head())) {
        String name = head_name->as_text();
        SyntaxInformation info(name);
        
        pmath_debug_print("[auto complete at arg %d", num_args_before + 1);
        pmath_debug_print_object(" of call to ", name.get(), "]\n");
        
        // TODO: >= info.minargs, but then maybe not an option unless also >= info.maxargs
        if(num_args_before >= info.maxargs) {
          Expr options = Application::interrupt_wait_cached(
                           Call(Symbol(richmath_System_Options), name));
          
          size_t num_opt = options.expr_length();
          Expr sym_opts    = MakeCall(Symbol(richmath_System_List), num_opt);
          Expr string_opts = MakeCall(Symbol(richmath_System_List), num_opt);
          size_t num_sym_opts    = 0;
          size_t num_string_opts = 0;
          for(size_t i = 1; i <= num_opt; ++i) {
            Expr rule = options[i];
            if(rule.is_rule()) {
              Expr lhs = rule[1];
              if(lhs.is_symbol()) {
                if(kind == CompletionLocation::Symbol)
                  sym_opts.set(++num_sym_opts, String(pmath_symbol_name(lhs.get())));
              }
              else if(lhs.is_string()) {
                String str = lhs.to_string(PMATH_WRITE_OPTIONS_FULLSTR);
                
                if(kind == CompletionLocation::InsideString) {
                  // Effectively str := FE`Private`MakeEscapedStringContent(str)
                  int len = str.length();
                  if(len >= 2 && str[0] == '"' && str[len - 1] == '"')
                    str = str.part(1, len - 2);
                }
                
                string_opts.set(++num_string_opts, str);
              }
            }
          }
          
          if(num_sym_opts > 0 || num_string_opts > 0) {
            if(num_sym_opts    < num_opt) sym_opts    = Expr(pmath_expr_get_item_range(sym_opts.get(),    1, num_sym_opts));
            if(num_string_opts < num_opt) string_opts = Expr(pmath_expr_get_item_range(string_opts.get(), 1, num_string_opts));
            
            Expr arg = Call(Symbol(richmath_System_PureArgument), Expr(1));
            Expr body_syms, body_strings;
            if(num_sym_opts > 0) 
              body_syms = Call(Symbol(richmath_FE_AutoCompleteName), sym_opts, arg);
            if(num_string_opts > 0) 
              body_strings =  Call(Symbol(richmath_FE_AutoCompleteOther), string_opts, arg);
            
            Expr body;
            if(body_syms && body_strings)
              body = Call(Symbol(richmath_System_Join), PMATH_CPP_MOVE(body_syms), PMATH_CPP_MOVE(body_strings));
            else
              body = body_syms ? body_syms : body_strings;
            
            return Call(Symbol(richmath_System_Function), PMATH_CPP_MOVE(body));
          }
        }
        
        // TODO: allow lists inside function arguments ...
        break;
      }
    }
    else if(!FunctionCallSpan::is_sequence(span))
      break;
  };
  return Expr();
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

bool AutoCompletion::is_active() const {
  if(!range.id)
    return false;
  
  if(priv->popup_id) {
    if(!FrontEndObject::find(priv->popup_id)) {
      // Auto completion was active with a popup, but the popup was closed.
      priv->remove_popup();
      //range.reset();
      return false;
    }
  }
  
  return true;
}

bool AutoCompletion::has_popup() const {
  if(!is_active())
    return false;
  
  return priv->popup_id.is_valid();
}

bool AutoCompletion::has_popup(FrontEndReference id) const {
  if(!id.is_valid() || !is_active())
    return false;
  
  return priv->popup_id.is_valid();
}

bool AutoCompletion::handle_key_backspace() {
  VolatileSelection dst = range.get_all();
  if(!dst)
    return false;
  
  if(dst.length() <= 1)
    return false;
  
  if(!priv->popup_id)
    return false;
  
  Document *popup = FrontEndObject::find_cast<Document>(priv->popup_id);
  if(!popup) { // popup was closed
    stop();
    return false;
  }
  
  VolatileLocation end = dst.end_only();
  if(VolatileSelection(end) != priv->document->selection_now())
    return false;
  
  end.move_logical_inplace(LogicalDirection::Backward, false);
  if(end.box != dst.box || end.index <= dst.start || dst.end <= end.index)
    return false;
  
  SelectionReference rem_sel {range.id, end.index, dst.end};
  
  AbstractSequence *seq = dynamic_cast<AbstractSequence*>(dst.box);
  if(!seq || !seq->editable() /*|| !seq->edit_selection(rem_sel, EditAction::DoIt)*/) {
    stop();
    return false;
  }
  
  //priv->document->native()->on_editing();
  
  {
    AutoValueReset<bool> continue_editing(priv->no_stop_while_editing);
    priv->no_stop_while_editing = true; 
    
    seq->remove(rem_sel.start, rem_sel.end);
    
    range = SelectionReference(seq->id(), range.start, rem_sel.start);
    priv->document->move_to(seq, rem_sel.start);
    
    popup->native()->source_range(range);
    // TODO: also re-adjust `anchor` in document->_attached_popup_windows[...].
  }
  
  if(!priv->reapply_filter()) {
    stop();
    return true; // already inserted character
  }

  return true;
}

bool AutoCompletion::handle_key_escape() {
  if(!range.id)
    return false;
  
  stop();
  return true;
}

bool AutoCompletion::handle_key_press(uint32_t unichar) {
  VolatileSelection dst = range.get_all();
  if(!dst)
    return false;
  
  if(!priv->popup_id)
    return false;
  
  Document *popup = FrontEndObject::find_cast<Document>(priv->popup_id);
  if(!popup) { // popup was closed
    stop();
    return false;
  }
  
  if(unichar == '\n') {
    if(priv->set_current_completion_text()) {
      stop();
    }
    return true;
  }
  
  if(unichar == PMATH_CHAR_BOX)
    return false;
  
  if(VolatileSelection(dst.end_only()) != priv->document->selection_now())
    return false;
  
  // TODO: Some characters might act as auto-fill-up characters, others as auto-cancel
  AbstractSequence *seq = dynamic_cast<AbstractSequence*>(dst.box);
  if(!seq || !seq->editable() /*|| !seq->edit_selection(range, EditAction::DoIt)*/) {
    stop();
    return false;
  }
  
  //priv->document->native()->on_editing();
  
  {
    AutoValueReset<bool> continue_editing(priv->no_stop_while_editing);
    priv->no_stop_while_editing = true; 
    
    int end = seq->insert(dst.end, unichar);
    range = SelectionReference(seq->id(), range.start, end);
    priv->document->move_to(seq, end);
    popup->native()->source_range(range);
    // TODO: also re-adjust `anchor` in document->_attached_popup_windows[...].
  }
  
  if(!priv->reapply_filter()) {
    stop();
    return true; // already inserted character
  }

  return true;
}

bool AutoCompletion::handle_key_tab(LogicalDirection direction) {
  if(!range.id) {
    Box *selbox = priv->document->selection_box();
    if(!selbox || !selbox->editable())
      return false;
      
    return priv->start(CompletionStyle::InlineCycle, direction);
  }
  else {
    if(priv->popup_id) {
      if(!FrontEndObject::find(priv->popup_id)) { // popup was closed
        stop();
        return false;
      }
      
      if(priv->set_current_completion_text()) {
        stop();
      }
      return true;
    }
    if(priv->continue_completion(direction))
      return priv->set_current_completion_text();
  }
  
  return false;
}

bool AutoCompletion::handle_key_up_down(LogicalDirection direction) {
  if(!range.id)
    return false;
  
  if(!priv->popup_id)
    return false;
  
  if(!FrontEndObject::find(priv->popup_id)) { // popup was closed
    stop();
    return false;
  }
  
  return priv->continue_completion(direction);
}

void AutoCompletion::stop() {
  if(priv->no_stop_while_editing && range)
    return;
  
  priv->current_boxes_list = Expr{};
  priv->current_filter_function = Expr{};
  priv->remove_popup();
  range.get_all().request_repaint();
  range.reset();
}

//} ... class AutoCompletion

