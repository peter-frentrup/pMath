#include <boxes/box-factory.h>

#include <boxes/graphics/graphicsbox.h>
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
#include <boxes/mathsequence.h>
#include <boxes/numberbox.h>
#include <boxes/openerbox.h>
#include <boxes/ownerbox.h>
#include <boxes/panebox.h>
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
#include <boxes/textsequence.h>
#include <boxes/tooltipbox.h>
#include <boxes/transformationbox.h>
#include <boxes/underoverscriptbox.h>


using namespace richmath;

extern pmath_symbol_t richmath_System_BoxData;
extern pmath_symbol_t richmath_System_ButtonBox;
extern pmath_symbol_t richmath_System_CheckboxBox;
extern pmath_symbol_t richmath_System_DynamicBox;
extern pmath_symbol_t richmath_System_DynamicLocalBox;
extern pmath_symbol_t richmath_System_FillBox;
extern pmath_symbol_t richmath_System_FractionBox;
extern pmath_symbol_t richmath_System_FrameBox;
extern pmath_symbol_t richmath_System_GraphicsBox;
extern pmath_symbol_t richmath_System_GridBox;
extern pmath_symbol_t richmath_System_InputFieldBox;
extern pmath_symbol_t richmath_System_InterpretationBox;
extern pmath_symbol_t richmath_System_List;
extern pmath_symbol_t richmath_System_OpenerBox;
extern pmath_symbol_t richmath_System_OverscriptBox;
extern pmath_symbol_t richmath_System_PaneBox;
extern pmath_symbol_t richmath_System_PanelBox;
extern pmath_symbol_t richmath_System_PaneSelectorBox;
extern pmath_symbol_t richmath_System_ProgressIndicatorBox;
extern pmath_symbol_t richmath_System_RadicalBox;
extern pmath_symbol_t richmath_System_RadioButtonBox;
extern pmath_symbol_t richmath_System_RotationBox;
extern pmath_symbol_t richmath_System_Section;
extern pmath_symbol_t richmath_System_SetterBox;
extern pmath_symbol_t richmath_System_SliderBox;
extern pmath_symbol_t richmath_System_SqrtBox;
extern pmath_symbol_t richmath_System_StringBox;
extern pmath_symbol_t richmath_System_StyleBox;
extern pmath_symbol_t richmath_System_StyleData;
extern pmath_symbol_t richmath_System_SubscriptBox;
extern pmath_symbol_t richmath_System_SubsuperscriptBox;
extern pmath_symbol_t richmath_System_SuperscriptBox;
extern pmath_symbol_t richmath_System_TagBox;
extern pmath_symbol_t richmath_System_TemplateBox;
extern pmath_symbol_t richmath_System_TemplateSlot;
extern pmath_symbol_t richmath_System_TextData;
extern pmath_symbol_t richmath_System_TooltipBox;
extern pmath_symbol_t richmath_System_TransformationBox;
extern pmath_symbol_t richmath_System_UnderscriptBox;
extern pmath_symbol_t richmath_System_UnderoverscriptBox;


static Box *finish_create_or_error(Box *box, Expr expr, BoxInputFlags options) {
  if(!box->try_load_from_object(expr, options)) {
    box->safe_destroy();
    return new ErrorBox(expr);
  }
  else
    return box;
}

//{ class BoxFactory ...

Box *BoxFactory::create_box(LayoutKind layout_kind, Expr expr, BoxInputFlags options) {
  
  if(String s = expr) {
    if(s.length() == 1 && s[0] == PMATH_CHAR_BOX)
      return new ErrorBox(s);
    
    InlineSequenceBox *box = new InlineSequenceBox(create_sequence(layout_kind));
    box->content()->load_from_object(s, options);
    return box;
  }
  
  if(!expr.is_expr())
    return new ErrorBox(expr);
    
  Expr head = expr[0];
  
  if(head == richmath_System_List || head == richmath_System_StringBox) {
    if(expr.expr_length() == 1) {
      expr = expr[1];
      return create_box(layout_kind, expr, options);
    }
    
    InlineSequenceBox *box = new InlineSequenceBox(new MathSequence);
    box->content()->load_from_object(std::move(expr), options);
    return box;
  }
  
  if(head == richmath_System_BoxData) 
    return finish_create_or_error(new InlineSequenceBox(new MathSequence), std::move(expr), options);
  
  if(head == richmath_System_TextData) 
    return finish_create_or_error(new InlineSequenceBox(new TextSequence), std::move(expr), options);
  
  if(head == richmath_System_ButtonBox)
    return finish_create_or_error(new ButtonBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_CheckboxBox)
    return finish_create_or_error(new CheckboxBox(), std::move(expr), options);
    
  if(head == richmath_System_DynamicBox)
    return finish_create_or_error(new DynamicBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_DynamicLocalBox)
    return finish_create_or_error(new DynamicLocalBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_FillBox)
    return finish_create_or_error(new FillBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_FractionBox)
    return finish_create_or_error(new FractionBox(), std::move(expr), options);
    
  if(head == richmath_System_FrameBox)
    return finish_create_or_error(new FrameBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_GraphicsBox)
    return finish_create_or_error(new GraphicsBox(), std::move(expr), options);
    
  if(head == richmath_System_GridBox)
    return finish_create_or_error(new GridBox(), std::move(expr), options);
    
  if(head == richmath_System_InputFieldBox)
    return finish_create_or_error(new InputFieldBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_InterpretationBox)
    return finish_create_or_error(new InterpretationBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_PaneBox)
    return finish_create_or_error(new PaneBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_PanelBox)
    return finish_create_or_error(new PanelBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_PaneSelectorBox)
    return finish_create_or_error(new PaneSelectorBox(), std::move(expr), options);
    
  if(head == richmath_System_ProgressIndicatorBox)
    return finish_create_or_error(new ProgressIndicatorBox(), std::move(expr), options);
    
  if(head == richmath_System_RadicalBox)
    return finish_create_or_error(new RadicalBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_RadioButtonBox)
    return finish_create_or_error(new RadioButtonBox(), std::move(expr), options);
    
  if(head == richmath_System_RotationBox)
    return finish_create_or_error(new RotationBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_SetterBox)
    return finish_create_or_error(new SetterBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_SliderBox)
    return finish_create_or_error(new SliderBox(), std::move(expr), options);
    
  if(head == richmath_System_SubscriptBox)
    return finish_create_or_error(new SubsuperscriptBox(), std::move(expr), options);
    
  if(head == richmath_System_SubsuperscriptBox)
    return finish_create_or_error(new SubsuperscriptBox(), std::move(expr), options);
    
  if(head == richmath_System_SuperscriptBox)
    return finish_create_or_error(new SubsuperscriptBox(), std::move(expr), options);
    
  if(head == richmath_System_SqrtBox)
    return finish_create_or_error(new RadicalBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_StyleBox)
    return finish_create_or_error(new StyleBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_TagBox)
    return finish_create_or_error(new TagBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_TemplateBox)
    return finish_create_or_error(new TemplateBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_TooltipBox)
    return finish_create_or_error(new TooltipBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_TransformationBox)
    return finish_create_or_error(new TransformationBox(create_sequence(layout_kind)), std::move(expr), options);
    
  if(head == richmath_System_OpenerBox)
    return finish_create_or_error(new OpenerBox(), std::move(expr), options);
    
  if(head == richmath_System_OverscriptBox)
    return finish_create_or_error(new UnderoverscriptBox(new MathSequence, nullptr, new MathSequence), std::move(expr), options);
    
  if(head == richmath_System_UnderoverscriptBox)
    return finish_create_or_error(new UnderoverscriptBox(new MathSequence, new MathSequence, new MathSequence), std::move(expr), options);
    
  if(head == richmath_System_UnderscriptBox)
    return finish_create_or_error(new UnderoverscriptBox(new MathSequence, new MathSequence, nullptr), std::move(expr), options);
    
  if(head == richmath_FE_NumberBox)
    return finish_create_or_error(new NumberBox(), std::move(expr), options);
    
  if(head == richmath_System_TemplateSlot)
    return finish_create_or_error(new TemplateBoxSlot(create_sequence(layout_kind)), std::move(expr), options);
    
  return new ErrorBox(std::move(expr));
}

AbstractSequence *BoxFactory::create_sequence(LayoutKind kind) {
  switch(kind) {
    case LayoutKind::Math: return new MathSequence;
    case LayoutKind::Text: return new TextSequence;
  }
  
  assert(0 && "not reached");
  return new MathSequence;
}

Section *BoxFactory::create_section(Expr expr) {
  if(expr[0] == richmath_System_Section) {
    Expr content = expr[1];
    
    Section *section = nullptr;
    
    if(content[0] == richmath_System_BoxData)
      section = new MathSection();
    else if(content[0] == richmath_System_StyleData)
      section = new StyleDataSection();
    else if(content[0] == richmath_System_TextData || content.is_string() || content[0] == richmath_System_List)
      section = new TextSection();
    
    if(section) {
      if(section->try_load_from_object(expr, BoxInputFlags::Default))
        return section;
      
      section->safe_destroy();
    }
  }
  
  return new ErrorSection(expr);
}

//} ... class BoxFactory
