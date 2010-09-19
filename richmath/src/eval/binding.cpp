#include <eval/binding.h>

#include <boxes/section.h>
#include <boxes/mathsequence.h>
#include <eval/client.h>
#include <eval/job.h>
#include <gui/document.h>
#include <gui/native-widget.h>

using namespace richmath;

Hashtable<int, Void, cast_hash> richmath::all_document_ids;

static int current_document_id = 0;

//{ pmath functions ...

static pmath_t builtin_addconfigshaper(pmath_expr_t expr){
  Expr data = Expr(
    pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_GET), 1,
        pmath_expr_get_item(expr, 1))));
  
  pmath_unref(expr);
  
  Client::notify(CNT_ADDCONFIGSHAPER, data);
  return NULL;
}

static pmath_t builtin_documentapply(pmath_expr_t _expr){
  Expr expr(_expr);
  
  if(expr.expr_length() != 2){
    pmath_message_argxxx(expr.expr_length(), 2, 2);
    return expr.release();
  }
  
  Client::notify_wait(CNT_MENUCOMMAND, expr);
  
  return NULL;
}

static pmath_t builtin_documents(pmath_expr_t expr){
  if(pmath_expr_length(expr) > 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return Client::notify_wait(CNT_GETDOCUMENTS, Expr()).release();
}

static pmath_t builtin_internal_dynamicupdated(pmath_expr_t expr){
  Client::notify(CNT_DYNAMICUPDATE, Expr(expr));
  return NULL;
}

static pmath_t builtin_feo_options(pmath_expr_t _expr){
  Expr expr(_expr);
  
  if(expr[0] == PMATH_SYMBOL_OPTIONS
  && expr.expr_length() == 1){
    Expr opts = Client::notify_wait(CNT_GETOPTIONS, expr[1]);
    
    return opts.release();
  }
  
  return expr.release();
}

static pmath_t builtin_frontendtokenexecute(pmath_expr_t expr){
  if(pmath_expr_length(expr) != 1){
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  Expr cmd = Expr(pmath_expr_get_item(expr, 1));
  pmath_unref(expr);
  
  Client::run_menucommand(cmd);
  
  return NULL;
}

static pmath_t builtin_sectionprint(pmath_expr_t expr){
  if(pmath_expr_length(expr) == 2){
    pmath_t style = pmath_expr_get_item(expr, 1);
    pmath_t boxes = pmath_expr_get_item(expr, 2);
    
    pmath_unref(expr);
    
    boxes = pmath_evaluate(
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_TOBOXES), 1,
        boxes));
    
    expr = pmath_expr_new_extended(
      pmath_ref(PMATH_SYMBOL_SECTION), 3,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_BOXDATA), 1,
        boxes),
      style,
      pmath_expr_new_extended(
        pmath_ref(PMATH_SYMBOL_RULE), 2,
        pmath_ref(PMATH_SYMBOL_SECTIONGENERATED),
        pmath_ref(PMATH_SYMBOL_TRUE)));
    
    Client::notify_wait(CNT_PRINTSECTION, Expr(expr));
    
    return NULL;
  }
  
  return expr;
}

static pmath_t builtin_selecteddocument(pmath_expr_t expr){
  if(pmath_expr_length(expr) > 0){
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  
  pmath_unref(expr);
  
  return pmath_expr_new_extended(
    pmath_ref(PMATH_SYMBOL_FRONTENDOBJECT), 1,
    pmath_integer_new_si(current_document_id));
}

//} ... pmath functions

//{ menu commands ...

static bool close_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->native()->close();
  return true;
}


static bool can_copy_cut(Expr cmd){
  Document *doc = get_current_document();
  
  return doc && doc->selection_length() > 0;
}

static bool copy_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->copy_to_clipboard();
  return true;
}

static bool cut_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->cut_to_clipboard();
  return true;
}

static bool paste_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->paste_from_clipboard();
  return true;
}

static bool can_edit_boxes(Expr cmd){
  Document *doc = get_current_document();
  
  return doc && (doc->selection_length() > 0 || doc->selection_box() != doc);
}

static bool edit_boxes_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc || !doc->selectable(-1))
    return false;
  
  int a, b;
  a = doc->selection_start();
  b = doc->selection_end();
  Box *box = doc->selection_box();
  
  while(box){
    if(box == doc)
      break;
    a = box->index();
    b = a + 1;
    box = box->parent();
  }
  
  if(box == doc){
    doc->select(0,0,0);
    
    for(int i = a;i < b;++i){
      EditSection *edit = dynamic_cast<EditSection*>(doc->section(i));
      pmath_continue_after_abort();
      
      if(edit){
        Expr parsed(edit->to_pmath(false));
        
        if(parsed == 0){
          doc->native()->beep();//MessageBeep(MB_ICONEXCLAMATION);
        }
        else{
          Section *sect = Section::create_from_object(parsed);
          sect->swap_id(edit);
          
          delete doc->swap(i, sect);
        }
      }
      else{
        Section *sect = doc->section(i);
        edit = new EditSection;
        if(!edit->style)
          edit->style = new Style;
        edit->style->set(SectionGroupPrecedence, sect->get_style(SectionGroupPrecedence));
        edit->swap_id(sect);
        edit->original = doc->swap(i, edit);
        
        Expr obj(sect->to_pmath(false));
        
        doc->select(edit->content(), 0, 0);
        doc->insert_string(obj.to_string(
          PMATH_WRITE_OPTIONS_FULLSTR | PMATH_WRITE_OPTIONS_INPUTEXPR));
      }
    }
  
    doc->select_range(box, a, a, box, b, b);
  }
  
  return true;
}

static bool expand_selection_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  Box *box  = doc->selection_box();
  int start = doc->selection_start();
  int end   = doc->selection_end();
  
  box = expand_selection(box, &start, &end);
  doc->select_range(box, start, start, box, end, end);
  doc->native()->invalidate();
  return true;
}

static bool find_matching_fence_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  MathSequence *seq = dynamic_cast<MathSequence*>(doc->selection_box());
  
  if(seq){
    int pos = doc->selection_end() - 1;
    int match = seq->matching_fence(pos);
    
    if(match < 0 && doc->selection_start() == doc->selection_end()){
      ++pos;
      match = seq->matching_fence(pos);
    }
    
    if(match >= 0){
      doc->select(seq, match, match + 1);
    }
  }
  
  return true;
}

static bool select_all_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  if(doc->selectable()){
    doc->select(doc, 0, doc->length());
    return true;
  }
  
  Box *sel = doc->selection_box();
  if(!sel)
    return false;
  
  Box *next = sel;
  while(next && next->selectable()){
    sel = next;
    next = next->parent();
  }
  
  if(sel->selectable()){
    doc->select(sel, 0, sel->length());
    return true;
  }
  
  return false;
}



static bool insert_column_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_matrix_column();
  return true;
}

static bool insert_fraction_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_fraction();
  return true;
}

static bool duplicate_previous_input_output_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  if(doc->selection_box() == doc && doc->selection_length() > 0)
    return false;
  
  Box *box = doc->selection_box();
  int a = doc->selection_start();
  int b = doc->selection_end();
  while(box && box != doc){
    a = box->index();
    b = a + 1;
    box = box->parent();
  }
  
  bool input = String(cmd).equals("DuplicatePreviousInput");
  
  for(int i = a - 1;i >= 0;--i){
    MathSection *math = dynamic_cast<MathSection*>(doc->item(i));
    
    if(math && ((input && math->get_style(BaseStyleName).equals("Input"))
     || (!input && math->get_style(SectionGenerated)))){
      MathSequence *seq = new MathSequence;
      seq->load_from_object(Expr(math->content()->to_pmath(false)), 0);
      doc->insert_box(seq);
      
      return true;
    }
  }
  
  doc->native()->beep();
  return true;
}

static bool similar_section_below_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  Box *box = doc->selection_box();
  while(box && box->parent() != doc){
    box = box->parent();
  }
  
  if(dynamic_cast<MathSection*>(box)){
    Style *style = new Style;
    style->merge(static_cast<Section*>(box)->style);
    style->remove(SectionLabel);
    style->remove(SectionGenerated);
    
    Section *section = new MathSection(style);
    
    doc->insert(box->index() + 1, section);
    doc->move_to(doc, box->index() + 1);
    doc->move_horizontal(Forward, false);
  }
  else
    doc->native()->beep();
  
  return true;
}

static bool insert_opposite_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->complete_box();
  return true;
}

static bool insert_overscript_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_underoverscript(false);
  return true;
}

static bool insert_radical_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_sqrt();
  return true;
}

static bool insert_row_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_matrix_row();
  return true;
}

static bool insert_subscript_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_subsuperscript(true);
  return true;
}

static bool insert_superscript_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_subsuperscript(false);
  return true;
}

static bool insert_underscript_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  doc->insert_underoverscript(true);
  return true;
}



static bool can_abort(Expr cmd){
  return !Client::is_idle();
}

static bool abort_cmd(Expr cmd){
  // non-blocking interrupt
  Client::execute_for(Call(Symbol(PMATH_SYMBOL_ABORT)), 0, Infinity);
  //Client::interrupt(Call(Symbol(PMATH_SYMBOL_ABORT)));
  //Client::abort_all();
  return true;
}

static bool evaluate_in_place_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  MathSequence *seq = dynamic_cast<MathSequence*>(doc->selection_box());
  
  if(seq){
    Client::add_job(new ReplacementJob(
      seq, 
      doc->selection_start(),
      doc->selection_end()));
  }
  
  return true;
}

static bool evaluate_sections_cmd(Expr cmd){
  Document *doc = get_current_document();
  
  if(!doc)
    return false;
  
  Box *box = doc->selection_box();
  
  if(box == doc){
    for(int i = doc->selection_start();i < doc->selection_end();++i){
      MathSection *math = dynamic_cast<MathSection*>(doc->item(i));
      
      if(math && math->get_style(BaseStyleName).equals("Input"))
        Client::add_job(new InputJob(math));
    }
  }
  else{
    while(box && !dynamic_cast<MathSection*>(box))
      box = box->parent();
    
    MathSection *math = dynamic_cast<MathSection*>(box);
    if(math && math->get_style(BaseStyleName).equals("Input"))
      Client::add_job(new InputJob(math));
  }
  
  if(String(cmd).equals("EvaluateSectionsAndReturn")){
    Client::add_job(new EvaluationJob(Call(Symbol(PMATH_SYMBOL_RETURN))));
  }
  
  return true;
}

static bool evaluator_subsession_cmd(Expr cmd){
  if(Client::is_idle())
    return false;
  
  // non-blocking interrupt
  Client::execute_for(Call(Symbol(PMATH_SYMBOL_DIALOG)), 0, Infinity);
  
  return true;
}

static bool subsession_evaluate_sections_cmd(Expr cmd){
  // non-blocking interrupt
  Client::execute_for(
    Call(Symbol(PMATH_SYMBOL_DIALOG),
      Call(Symbol(PMATH_SYMBOL_FRONTENDTOKENEXECUTE), 
        String("EvaluateSectionsAndReturn"))), 
    0, Infinity);
  
  return false;
}


static bool cmd_document_apply(Expr cmd){
  Document *doc = dynamic_cast<Document*>(Box::find(cmd[1]));
  
  if(!doc)
    return false;
  
  MathSequence *seq = new MathSequence;
  seq->load_from_object(cmd[2], BoxOptionDefault);
  doc->insert_box(seq, true);
  
  return true;
}

//} ... menu commands

static pmath_symbol_t fe_symbols[FrontEndSymbolsCount];

bool richmath::init_bindings(){
  #define BIND_DOWN(SYMBOL, FUNC)  pmath_register_code(SYMBOL, FUNC, PMATH_CODE_USAGE_DOWNCALL)
  #define BIND_UP(  SYMBOL, FUNC)  pmath_register_code(SYMBOL, FUNC, PMATH_CODE_USAGE_UPCALL)
  
//  if(!BIND_DOWN(PMATH_SYMBOL_SECTIONPRINT, builtin_sectionprint)
//  || !BIND_DOWN(PMATH_SYMBOL_DOCUMENTS,    builtin_documents))
//    return false;
  
  Client::register_menucommand(String("Close"),             close_cmd);
  
  Client::register_menucommand(String("Copy"),              copy_cmd,                 can_copy_cut);
  Client::register_menucommand(String("Cut"),               cut_cmd,                  can_copy_cut);
  Client::register_menucommand(String("Paste"),             paste_cmd);
  Client::register_menucommand(String("EditBoxes"),         edit_boxes_cmd,           can_edit_boxes);
  Client::register_menucommand(String("ExpandSelection"),   expand_selection_cmd);
  Client::register_menucommand(String("FindMatchingFence"), find_matching_fence_cmd);
  Client::register_menucommand(String("SelectAll"),         select_all_cmd);
  
  Client::register_menucommand(String("DuplicatePreviousInput"),  duplicate_previous_input_output_cmd);
  Client::register_menucommand(String("DuplicatePreviousOutput"), duplicate_previous_input_output_cmd);
  Client::register_menucommand(String("InsertColumn"),            insert_column_cmd);
  Client::register_menucommand(String("InsertFraction"),          insert_fraction_cmd);
  Client::register_menucommand(String("InsertOpposite"),          insert_opposite_cmd);
  Client::register_menucommand(String("InsertOverscript"),        insert_overscript_cmd);
  Client::register_menucommand(String("InsertRadical"),           insert_radical_cmd);
  Client::register_menucommand(String("InsertRow"),               insert_row_cmd);
  Client::register_menucommand(String("InsertSubscript"),         insert_subscript_cmd);
  Client::register_menucommand(String("InsertSuperscript"),       insert_superscript_cmd);
  Client::register_menucommand(String("InsertUnderscript"),       insert_underscript_cmd);
  Client::register_menucommand(String("SimilarSectionBelow"),     similar_section_below_cmd);
  
  Client::register_menucommand(String("EvaluatorAbort"),             abort_cmd,                 can_abort);
  Client::register_menucommand(String("EvaluateInPlace"),            evaluate_in_place_cmd);
  Client::register_menucommand(String("EvaluateSections"),           evaluate_sections_cmd);
  Client::register_menucommand(String("EvaluateSectionsAndReturn"),  evaluate_sections_cmd);
  Client::register_menucommand(String("EvaluatorSubsession"),        evaluator_subsession_cmd,  can_abort);
  Client::register_menucommand(String("SubsessionEvaluateSections"), subsession_evaluate_sections_cmd);
  
  Client::register_menucommand(Symbol(PMATH_SYMBOL_DOCUMENTAPPLY), cmd_document_apply);
  
  #define VERIFY(x)  if(0 == (x)) goto FAIL_SYMBOLS;
  #define NEW_SYMBOL(name)     pmath_symbol_get(PMATH_C_STRING(name), TRUE)
  
  memset(fe_symbols, 0, sizeof(fe_symbols));
  VERIFY(fe_symbols[NumberBoxSymbol]       = NEW_SYMBOL("FE`NumberBox"))
  VERIFY(fe_symbols[SymbolInfoSymbol]      = NEW_SYMBOL("FE`SymbolInfo"))
  VERIFY(fe_symbols[AddConfigShaperSymbol] = NEW_SYMBOL("FE`AddConfigShaper"))
  
  VERIFY(BIND_DOWN(PMATH_SYMBOL_INTERNAL_DYNAMICUPDATED, builtin_internal_dynamicupdated))
  
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTAPPLY,        builtin_documentapply))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_DOCUMENTS,            builtin_documents))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_FRONTENDTOKENEXECUTE, builtin_frontendtokenexecute))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_SECTIONPRINT,         builtin_sectionprint))
  VERIFY(BIND_DOWN(PMATH_SYMBOL_SELECTEDDOCUMENT,     builtin_selecteddocument))
  
  VERIFY(BIND_UP(  PMATH_SYMBOL_FRONTENDOBJECT, builtin_feo_options))
  
  VERIFY(BIND_DOWN(fe_symbols[AddConfigShaperSymbol], builtin_addconfigshaper))

  return true;
  
 FAIL_SYMBOLS:
  for(size_t i = 0;i < FrontEndSymbolsCount;++i)
    pmath_unref(fe_symbols[i]);
    
  memset(fe_symbols, 0, sizeof(fe_symbols));
  return false;
}

void richmath::done_bindings(){
  for(size_t i = 0;i < FrontEndSymbolsCount;++i)
    pmath_unref(fe_symbols[i]);
  
  memset(fe_symbols, 0, sizeof(fe_symbols));
}

pmath_symbol_t richmath::GetSymbol(FrontEndSymbolIndex i){
  if((size_t)i >= (size_t)FrontEndSymbolsCount)
    return NULL;
  
  return fe_symbols[(size_t)i];
}

void richmath::set_current_document(Document *document){
  Document *old = get_current_document();
  
  if(old)
    old->focus_killed();
  
  if(document)
    document->focus_set();
    
  current_document_id = document ? document->id() : 0;
}

Document *richmath::get_current_document(){
  return dynamic_cast<Document*>(Box::find(current_document_id));
}
