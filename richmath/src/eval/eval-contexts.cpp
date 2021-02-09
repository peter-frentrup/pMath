#include <eval/eval-contexts.h>
#include <eval/current-value.h>

#include <boxes/section.h>
#include <boxes/sectionlist.h>

#include <gui/document.h>


namespace richmath { namespace strings {
  extern String DollarEvaluationContext_namespace;
  extern String Global_namespace;
}}

extern pmath_symbol_t richmath_System_Document;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SectionGroup;
extern pmath_symbol_t richmath_FE_Private_DeleteEvaluationContext;
extern pmath_symbol_t richmath_FE_Private_SwitchEvaluationContext;

using namespace richmath;

namespace richmath {
  class EvaluationContexts::Impl {
    public:
      template<class T> 
      static T *find_parent_or_self(StyledObject *obj) {
        while(obj) {
          if(T *result = dynamic_cast<T*>(obj))
            return result;
          
          obj = obj->style_parent();
        }
        
        return nullptr;
      }
      
      static String generate_context_name(StyledObject *obj);
      static String generate_context_name_and_remember(StyledObject *obj) { remember_that_object_defines_context(obj); return generate_context_name(obj); }
    
    private:
      static void remember_that_object_defines_context(StyledObject *obj);
      
    public:
      static String current_context;
  };
}

String EvaluationContexts::Impl::current_context;

//{ class EvaluationContexts ...

void EvaluationContexts::init() {
  Impl::current_context = strings::Global_namespace;
  
  CurrentValue::register_provider(
    String("ResolvedEvaluationContext"), 
    [](FrontEndObject *obj, Expr item) -> Expr { return resolve_context(dynamic_cast<StyledObject*>(obj)); });
}

void EvaluationContexts::done() {
  Impl::current_context = String();
}

void EvaluationContexts::context_source_deleted(StyledObject *obj) {
  String ctx = Impl::generate_context_name(obj);
  
  if(ctx == strings::Global_namespace)
    return;
  
  if(ctx == Impl::current_context) 
    set_context(strings::Global_namespace);
  
  Application::async_interrupt(Call(Symbol(richmath_FE_Private_DeleteEvaluationContext), std::move(ctx)));
}

void EvaluationContexts::set_context(String context) {
  if(auto expr = prepare_set_context(context))
    Application::interrupt_wait(expr);
}

Expr EvaluationContexts::prepare_set_context(String context) {
  if(!context.is_namespace())
    return Expr();
  
  if(context == Impl::current_context)
    return Expr();
  
  auto old_ctx = std::move(Impl::current_context);
  Impl::current_context = context;
  return Call(Symbol(richmath_FE_Private_SwitchEvaluationContext), std::move(old_ctx), std::move(context));
}

Section *EvaluationContexts::find_section_group_header(Section *section) {
  if(!section)
    return nullptr;
  
  SectionList *slist = dynamic_cast<SectionList*>(section->parent());
  if(!slist)
    return nullptr;
  
  int i = section->index();
  i = slist->group_info(i).first; // index of owner of section.
  
  if(i < 0)
    return nullptr;
  
  if(section->get_own_style(SectionGenerated, false)) {
    // i was the input, go one step further.
    i = slist->group_info(i).first;
  }
  
  if(i < 0)
    return nullptr;
  
  return slist->section(i);
}

String EvaluationContexts::resolve_context(StyledObject *obj) {
  if(!obj)
    return strings::Global_namespace;
  
  Expr ctx = obj->get_style(EvaluationContext);
  if(ctx.is_namespace())
    return String(ctx);
  
  if(ctx == richmath_System_Document) {
    Document *doc = Impl::find_parent_or_self<Document>(obj);
    return Impl::generate_context_name_and_remember(doc);
  }
  
  Section *sect = Impl::find_parent_or_self<Section>(obj);
  if(ctx == richmath_System_Section) {
    return Impl::generate_context_name_and_remember(sect);
  }
  
  if(ctx == richmath_System_SectionGroup) {
    sect = find_section_group_header(sect);
    return Impl::generate_context_name_and_remember(sect);
  }
  
  return strings::Global_namespace;
}

static Expr replace_symbol_context(Expr expr, String old_ctx, String new_ctx) {
  if(old_ctx.is_namespace() && new_ctx.is_namespace()) {
    SymbolNamespaceReplacer repl(old_ctx, new_ctx);
    
    if(new_ctx == strings::DollarEvaluationContext_namespace) {
      repl.extra_attributes = PMATH_SYMBOL_ATTRIBUTE_TEMPORARY;
      //repl.copy_old_attributes = true;
    }
    
    return Expr(repl.run(expr.release()));
  }
  return expr;
}

//} ... class EvaluationContexts

//{ class EvaluationContexts::Impl ...

void EvaluationContexts::Impl::remember_that_object_defines_context(StyledObject *obj) {
  if(!obj)
    return;
  
  auto style = obj->own_style();
  if(!style) // TODO: create if necessary 
    return;
  
  style->set(InternalDefinesEvaluationContext, true);
}

String EvaluationContexts::Impl::generate_context_name(StyledObject *obj) {
  if(!obj)
    return strings::Global_namespace;
  
  String prefix;
  if(dynamic_cast<Document*>(obj))
    prefix = "Document$$";
  else if(dynamic_cast<Section*>(obj))
    prefix = "Section$$";
  else
    prefix = "Object$$";
  
  prefix+= obj->id().to_pmath_raw().to_string();
  
  return prefix + "`";
}

//} ... class EvaluationContexts::Impl
