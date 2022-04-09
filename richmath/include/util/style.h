#ifndef RICHMATH__UTIL__STYLE_H__INCLUDED
#define RICHMATH__UTIL__STYLE_H__INCLUDED

#include <eval/observable.h>
#include <graphics/color.h>
#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>

namespace richmath {
  class StyledObject;
  
  bool get_factor_of_scaled(Expr expr, double *value);
  
  enum class DefaultStyleOptionOffsets {
    None = 0,
    ButtonBox       = 0x00100000,
    DynamicBox      = 0x00200000,
    DynamicLocalBox = 0x00300000,
    FillBox         = 0x00400000,
    FrameBox        = 0x00500000,
    InputFieldBox   = 0x00600000,
    PaneBox         = 0x00700000,
    PanelBox        = 0x00800000,
    SetterBox       = 0x00900000,
    SliderBox       = 0x00A00000,
    TemplateBox     = 0x00B00000,
  };
  
  enum AutoBoolValues {
    AutoBoolFalse = 0,
    AutoBoolTrue = 1,
    AutoBoolAutomatic = 2
  };
  
  enum ClosingActionValues {
    ClosingActionDelete = 0,
    ClosingActionHide   = 1,
  };
  
  enum ControlPlacementKind {
    ControlPlacementKindBottom,
    ControlPlacementKindLeft,
    ControlPlacementKindRight,
    ControlPlacementKindTop,
  };
  
  enum ImageSizeActionValues {
    ImageSizeActionClip = 0,
    ImageSizeActionShrinkToFit = 1,
    ImageSizeActionResizeToFit = 2,
  };
  
  enum ObserverKind {
    ObserverKindNone  = 0x0,
    ObserverKindSelf  = 0x1,
    ObserverKindOther = 0x2,
    ObserverKindBoth  = ObserverKindSelf | ObserverKindOther,
  };
  
  enum RemovalConditionFlags {
    RemovalConditionFlagSelectionExit     = 0x01,
    RemovalConditionFlagMouseExit         = 0x02,
    RemovalConditionFlagOutsideMouseClick = 0x04,
    RemovalConditionFlagParentChanged     = 0x08,
    //RemovalConditionFlagFocusExit         = 0x10,
  };
  
  enum ColorStyleOptionName {
    Background = 0x00000,
    ColorForGraphics,
    FontColor,
    SectionFrameColor,
    
    CharacterNameSyntaxColor,
    CommentSyntaxColor,
    ExcessOrMissingArgumentSyntaxColor,
    FunctionLocalVariableSyntaxColor,
    FunctionNameSyntaxColor,
    ImplicitOperatorSyntaxColor,
    KeywordSymbolSyntaxColor,
    LocalScopeConflictSyntaxColor,
    LocalVariableSyntaxColor,
    PatternVariableSyntaxColor,
    SectionInsertionPointColor,
    StringSyntaxColor,
    SymbolShadowingSyntaxColor,
    SyntaxErrorColor,
    UndefinedSymbolSyntaxColor,
    UnknownOptionSyntaxColor,
    
    InlineAutoCompletionBackgroundColor,
    MatchingBracketBackgroundColor,
    OccurenceBackgroundColor,
    FrameBoxDefaultBackground = Background + (int)DefaultStyleOptionOffsets::FrameBox,
  };
  
  enum IntStyleOptionName {
    Antialiasing = 0x10000, // AutoBoolXXX
    AutoDelete,
    AutoNumberFormating,
    AutoSpacing,
    ClosingAction, // ClosingActionXXX
    ContentPadding,
    ContinuousAction,
    ControlPlacement, // ControlPlacementKindXXX
    DebugColorizeChanges, // bool
    DebugFollowMouse,     // bool
    DebugSelectionBounds, // bool
    Editable,
    Enabled, // AutoBoolXXX
    Evaluatable,
    ImageSizeAction, // ImageSizeActionXXX
    InternalDefinesEvaluationContext, // bool
    InternalHasModifiedWindowOption,
    InternalHasPendingDynamic,
    InternalHasNewBaseStyle,
    InternalRequiresChildResize,
    InternalUsesCurrentValueOfMouseOver, // ObserverKindXXX
    LineBreakWithin,
    MenuCommandKey,
    MenuSortingValue,
    Placeholder,
    RemovalConditions, // 0 or more of RemovalConditionFlagXXX
    ReturnCreatesNewSection,
    Saveable,
    ScriptLevel,
    SectionEditDuplicate,
    SectionEditDuplicateMakesCopy,
    SectionGenerated,
    SectionLabelAutoDelete,
    Selectable, // AutoBoolXXX
    ShowAutoStyles,
    ShowSectionBracket, // AutoBoolXXX
    ShowStringCharacters,
    StripOnInput,
    SurdForm,
    SynchronousUpdating, // AutoBoolXXX
    Visible,
    WholeSectionGroupOpener,
    
    FontSlant,
    FontWeight,
    
    ButtonFrame, // -1 = Automatic,  other: ContainerType value
    ButtonSource, // ButtonSourceXXX
    WindowFrame, // WindowFrameType
    
    ButtonBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultEnabled        = Enabled        + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonFrame    = ButtonFrame    + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonSource   = ButtonSource   + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    DynamicBoxDefaultSelectable          = Selectable          + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultSynchronousUpdating = SynchronousUpdating + (int)DefaultStyleOptionOffsets::DynamicBox,
    
    FillBoxDefaultStripOnInput = StripOnInput + (int)DefaultStyleOptionOffsets::FillBox,
    
    FrameBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::FrameBox,
    
    InputFieldBoxDefaultContinuousAction = ContinuousAction + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultEnabled          = Enabled          + (int)DefaultStyleOptionOffsets::InputFieldBox,
    
    PaneBoxDefaultImageSizeAction = ImageSizeAction + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultLineBreakWithin = LineBreakWithin + (int)DefaultStyleOptionOffsets::PaneBox,
    
    PanelBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultEnabled        = Enabled        + (int)DefaultStyleOptionOffsets::PanelBox,
    
    SetterBoxDefaultContentPadding = ContentPadding + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultEnabled        = Enabled        + (int)DefaultStyleOptionOffsets::SetterBox,
    SetterBoxDefaultButtonFrame    = ButtonFrame    + (int)DefaultStyleOptionOffsets::SetterBox,
    
    SliderBoxDefaultContinuousAction = ContinuousAction + (int)DefaultStyleOptionOffsets::SliderBox,
    SliderBoxDefaultEnabled          = Enabled          + (int)DefaultStyleOptionOffsets::SliderBox,
  };
  
  enum {
    ButtonSourceAutomatic      = 0,
    ButtonSourceButtonContents = 1,
    ButtonSourceButtonData     = 2,
    ButtonSourceButtonBox      = 3,
    ButtonSourceFrontEndObject = 4
  };
  
  enum {
    FontWeightPlain = 0,
    FontWeightBold  = 100
  };
  
  enum WindowFrameType {
    WindowFrameNormal      = 0,
    WindowFramePalette     = 1,
    WindowFrameDialog      = 2,
    WindowFrameNone        = 3,
    WindowFrameThin        = 4,
    WindowFrameThinCallout = 5,
  };
  
  static const float ImageSizeAutomatic = -1.0f;
  
  enum FloatStyleOptionName {
    FontSize = 0x20000, // greater than any IntStyleOptionName value
    
    AspectRatio,
    Magnification,
    
    FillBoxWeight, // > 0
    
    GridBoxColumnSpacing,
    GridBoxRowSpacing,
    
    ImageSizeCommon,
    ImageSizeHorizontal, // > 0 or ImageSizeAutomatic
    ImageSizeVertical,   // > 0 or ImageSizeAutomatic
    
    InlineAutoCompletionHighlightOpacity, // 0 .. 1
    MatchingBracketHighlightOpacity,      // 0 .. 1
    OccurenceHighlightOpacity,            // 0 .. 1
    
    SectionMarginLeft,
    SectionMarginRight,
    SectionMarginTop,
    SectionMarginBottom,
    
    SectionFrameLeft,
    SectionFrameRight,
    SectionFrameTop,
    SectionFrameBottom,
    
    SectionFrameMarginLeft,
    SectionFrameMarginRight,
    SectionFrameMarginTop,
    SectionFrameMarginBottom,
    
    SectionFrameLabelMarginLeft,   // used for CellDingbat
    SectionFrameLabelMarginRight,  // not yet used
    SectionFrameLabelMarginTop,    // not yet used
    SectionFrameLabelMarginBottom, // not yet used
    
    SectionGroupPrecedence,
    
    FillBoxDefaultFillBoxWeight = FillBoxWeight + (int)DefaultStyleOptionOffsets::FillBox,
    
    PaneBoxDefaultImageSizeCommon     = ImageSizeCommon     + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultImageSizeHorizontal = ImageSizeHorizontal + (int)DefaultStyleOptionOffsets::PaneBox,
    PaneBoxDefaultImageSizeVertical   = ImageSizeVertical   + (int)DefaultStyleOptionOffsets::PaneBox,
  };
  
  enum StringStyleOptionName {
    BaseStyleName = 0x30000, // greater than any FloatStyleOptionName value
    Method,
    
    LanguageCategory,
    SectionLabel,
    
    WindowTitle,
    
    ButtonBoxDefaultMethod = Method + (int)DefaultStyleOptionOffsets::ButtonBox
  };
  
  enum ObjectStyleOptionName {
    Axes = 0x40000, // greater than any StringStyleOptionName value
    Ticks,
    Frame,
    FrameTicks,
    AxesOrigin,
    BaselinePosition,
    ScriptSizeMultipliers,
    TextShadow,
    FontFeatures,
    MathFontFamily,
    UnknownOptions,
    EvaluationContext,
    
    ButtonData,
    ButtonFunction,
    
    BoxRotation,
    BoxTransformation,
    PlotRange,
    BorderRadius,
    FrameStyle,
    
    ContextMenu,         // StyleType::AnyFlatList
    DragDropContextMenu, // StyleType::AnyFlatList
    
    DefaultDuplicateSectionStyle,
    DefaultNewSectionStyle,
    DefaultReturnCreatedSectionStyle,
    
    DockedSections,
    DockedSectionsTop,         // StyleType::AnyFlatList
    DockedSectionsTopGlass,    // StyleType::AnyFlatList
    DockedSectionsBottom,      // StyleType::AnyFlatList
    DockedSectionsBottomGlass, // StyleType::AnyFlatList
    
    FontFamilies,          // StyleType::AnyFlatList
    InputAliases,          // StyleType::AnyFlatList
    InputAutoReplacements, // StyleType::AnyFlatList
    
    ButtonBoxOptions,
    DynamicBoxOptions,
    DynamicLocalBoxOptions,
    FillBoxOptions,
    FrameBoxOptions,
    InputFieldBoxOptions,
    PaneBoxOptions,
    PanelBoxOptions,
    SetterBoxOptions,
    SliderBoxOptions,
    TemplateBoxOptions,
    
    InternalDeinitialization,
    Deinitialization,
    Initialization,
    TrackedSymbols,
    DynamicLocalValues,
    UnsavedVariables,
    InternalUpdateCause,
    
    DisplayFunction,
    InterpretationFunction,
    SyntaxForm,
    Tooltip,
    
    StyleDefinitions,
    InternalLastStyleDefinitions,
    GeneratedSectionStyles,
    
    SectionDingbat,
    SectionEvaluationFunction,
    Appearance,
    
    CharacterNameStyle,
    CommentStyle,
    ExcessOrMissingArgumentStyle,
    FunctionLocalVariableStyle,
    FunctionNameStyle,
    ImplicitOperatorStyle,
    InlineAutoCompletionStyle,
    KeywordSymbolStyle,
    LocalScopeConflictStyle,
    LocalVariableStyle,
    MatchingBracketHighlightStyle,
    OccurenceHighlightStyle,
    PatternVariableStyle,
    StringStyle,
    SymbolShadowingStyle,
    SyntaxErrorStyle,
    UndefinedSymbolStyle,
    UnknownOptionStyle,
    
    ButtonBoxDefaultAppearance               = Appearance             + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultBaselinePosition         = BaselinePosition       + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonData               = ButtonData             + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonFunction           = ButtonFunction         + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    DynamicBoxDefaultDeinitialization        = Deinitialization       + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultInitialization          = Initialization         + (int)DefaultStyleOptionOffsets::DynamicBox,
    DynamicBoxDefaultTrackedSymbols          = TrackedSymbols         + (int)DefaultStyleOptionOffsets::DynamicBox,
    
    DynamicLocalBoxDefaultDeinitialization   = Deinitialization       + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    DynamicLocalBoxDefaultInitialization     = Initialization         + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    DynamicLocalBoxDefaultUnsavedVariables   = UnsavedVariables       + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    DynamicLocalBoxDefaultDynamicLocalValues = DynamicLocalValues     + (int)DefaultStyleOptionOffsets::DynamicLocalBox,
    
    FrameBoxDefaultBorderRadius              = BorderRadius           + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultBaselinePosition          = BaselinePosition       + (int)DefaultStyleOptionOffsets::FrameBox,
    FrameBoxDefaultFrameStyle                = FrameStyle             + (int)DefaultStyleOptionOffsets::FrameBox,
    
    InputFieldBoxDefaultAppearance           = Appearance             + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultBaselinePosition     = BaselinePosition       + (int)DefaultStyleOptionOffsets::InputFieldBox,
    
    PaneBoxDefaultBaselinePosition           = BaselinePosition       + (int)DefaultStyleOptionOffsets::PaneBox,
    
    PanelBoxDefaultAppearance                = Appearance             + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultBaselinePosition          = BaselinePosition       + (int)DefaultStyleOptionOffsets::PanelBox,
    
    SetterBoxDefaultBaselinePosition         = BaselinePosition       + (int)DefaultStyleOptionOffsets::SetterBox,
    
    SliderBoxDefaultAppearance               = Appearance             + (int)DefaultStyleOptionOffsets::SliderBox,
    
    TemplateBoxDefaultDisplayFunction        = DisplayFunction        + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultInterpretationFunction = InterpretationFunction + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultSyntaxForm             = SyntaxForm             + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultTooltip                = Tooltip                + (int)DefaultStyleOptionOffsets::TemplateBox,
  };
  
  class StyleOptionName {
      static const int DynamicFlag  = 0x10000000;
      static const int VolatileFlag = 0x20000000;
  
    public:
      StyleOptionName() = default;
      
      explicit StyleOptionName(int value) : _value(value) {}
      StyleOptionName(ColorStyleOptionName value) : _value((int)value) {}
      StyleOptionName(IntStyleOptionName value) : _value((int)value) {}
      StyleOptionName(FloatStyleOptionName value) : _value((int)value) {}
      StyleOptionName(StringStyleOptionName value) : _value((int)value) {}
      StyleOptionName(ObjectStyleOptionName value) : _value((int)value) {}
      
      explicit operator ColorStyleOptionName() const {
        return (ColorStyleOptionName)_value;
      }
      explicit operator IntStyleOptionName() const {
        return (IntStyleOptionName)_value;
      }
      explicit operator FloatStyleOptionName() const {
        return (FloatStyleOptionName)_value;
      }
      explicit operator StringStyleOptionName() const {
        return (StringStyleOptionName)_value;
      }
      explicit operator ObjectStyleOptionName() const {
        return (ObjectStyleOptionName)_value;
      }
      explicit operator int() const {
        return _value;
      }
      
      bool is_valid() const {
        return _value >= 0;
      }
      
      bool is_literal() const {
        return (_value & (DynamicFlag | VolatileFlag)) == 0;
      }
      bool is_dynamic() const {
        return (_value & DynamicFlag) != 0;
      }
      bool is_volatile() const {
        return (_value & VolatileFlag) != 0;
      }
      StyleOptionName to_literal() const {
        return StyleOptionName { _value & ~(DynamicFlag | VolatileFlag) };
      }
      StyleOptionName to_dynamic() const {
        return StyleOptionName { (_value & ~VolatileFlag) | DynamicFlag };
      }
      StyleOptionName to_volatile() const {
        return StyleOptionName { (_value & ~DynamicFlag) | VolatileFlag };
      }
      
      unsigned int hash() const {
        return (unsigned int)_value;
      }
  
      bool operator==(StyleOptionName other) const {
        return _value == other._value;
      }
      
      bool operator!=(StyleOptionName other) const {
        return _value != other._value;
      }
      
    private:
      int _value;
  };
  
  enum class StyleType {
    None,
    Bool,
    AutoBool,
    Color,
    Integer,
    Number,
    Margin,
    Size,
    String,
    Any,
    AnyFlatList,
    Enum,
    RuleSet
  };
  
  union IntFloatUnion {
    int   int_value;
    float float_value;
  };
  
  class Style: public Observable, public Shareable {
    friend class StyleImpl;
    public:
      Style();
      Style(Expr options);
      virtual ~Style();
      
      void clear();
      static void reset(SharedPtr<Style> &style, String base_style_name);
      
      void add_pmath(Expr options, bool amend = true);
      
      void merge(SharedPtr<Style> other);
      static bool contains_inherited(Expr expr);
      static Expr merge_style_values(StyleOptionName n, Expr newer, Expr older);
      static Expr finish_style_merge(StyleOptionName n, Expr value);
      
      bool contains(ColorStyleOptionName  n) const { Color  _; return get(n, &_); }
      bool contains(IntStyleOptionName    n) const { int    _; return get(n, &_); }
      bool contains(FloatStyleOptionName  n) const { float  _; return get(n, &_); }
      bool contains(StringStyleOptionName n) const { String _; return get(n, &_); }
      bool contains(ObjectStyleOptionName n) const { Expr   _; return get(n, &_); }
      
      virtual bool get(ColorStyleOptionName  n, Color  *value) const;
      virtual bool get(IntStyleOptionName    n, int    *value) const;
      virtual bool get(FloatStyleOptionName  n, float  *value) const;
      virtual bool get(StringStyleOptionName n, String *value) const;
      virtual bool get(ObjectStyleOptionName n, Expr   *value) const;
      
      virtual void set(ColorStyleOptionName  n, Color  value);
      virtual void set(IntStyleOptionName    n, int    value);
      virtual void set(FloatStyleOptionName  n, float  value);
      virtual void set(StringStyleOptionName n, String value);
      virtual void set(ObjectStyleOptionName n, Expr   value);
      
      virtual void remove(ColorStyleOptionName  n);
      virtual void remove(IntStyleOptionName    n);
      virtual void remove(FloatStyleOptionName  n);
      virtual void remove(StringStyleOptionName n);
      virtual void remove(ObjectStyleOptionName n);
      
      void flag_pending_dynamic() { set(InternalHasPendingDynamic, true); }
      
      bool get_dynamic(StyleOptionName n, Expr *value) const {
        return get((ObjectStyleOptionName)n.to_dynamic(), value);
      }
      
      unsigned int count() const;
      
      bool set_pmath(StyleOptionName n, Expr obj);
      
      Expr get_pmath(StyleOptionName n) const;
      
      void emit_pmath(StyleOptionName n) const;
      
      void emit_to_pmath(bool with_inherited = false) const;
      
      static bool modifies_size(StyleOptionName style_name);
      
      static bool is_style_name(Expr n);
      static StyleOptionName get_key(Expr n);
      
      static Expr get_name(StyleOptionName n);
      
      static enum StyleType get_type(StyleOptionName n);
      
      static Expr get_current_style_value(FrontEndObject *obj, Expr item);
      static bool put_current_style_value(FrontEndObject *obj, Expr item, Expr rhs);
      
    private:
      Hashtable<StyleOptionName, IntFloatUnion> int_float_values;
      Hashtable<StyleOptionName, Expr>          object_values;
  };
  
  class Stylesheet: public FrontEndObject, public Shareable {
      friend class StylesheetImpl;
    public:
      Stylesheet();
      virtual ~Stylesheet() override;
      
      SharedPtr<Style> find_parent_style(SharedPtr<Style> s);
      
      // each get() ignores base:
      bool get(SharedPtr<Style> s, ColorStyleOptionName  n, Color  *value);
      bool get(SharedPtr<Style> s, IntStyleOptionName    n, int    *value);
      bool get(SharedPtr<Style> s, FloatStyleOptionName  n, float  *value);
      bool get(SharedPtr<Style> s, StringStyleOptionName n, String *value);
      bool get(SharedPtr<Style> s, ObjectStyleOptionName n, Expr   *value);
      
      Expr get_pmath(SharedPtr<Style> s, StyleOptionName n);
      
      bool update_dynamic(SharedPtr<Style> s, StyledObject *parent);
      
      Color  get_or_default(SharedPtr<Style> s, ColorStyleOptionName n,  Color  fallback_result = Color::None) { get(std::move(s), n, &fallback_result); return fallback_result; }
      int    get_or_default(SharedPtr<Style> s, IntStyleOptionName n,    int    fallback_result = 0) {           get(std::move(s), n, &fallback_result); return fallback_result; }
      float  get_or_default(SharedPtr<Style> s, FloatStyleOptionName n,  float  fallback_result = 0.0f) {        get(std::move(s), n, &fallback_result); return fallback_result; }
      String get_or_default(SharedPtr<Style> s, StringStyleOptionName n, String fallback_result) {               get(std::move(s), n, &fallback_result); return fallback_result; }
      Expr   get_or_default(SharedPtr<Style> s, ObjectStyleOptionName n, Expr   fallback_result) {               get(std::move(s), n, &fallback_result); return fallback_result; }
      String get_or_default(SharedPtr<Style> s, StringStyleOptionName n) { return get_or_default(std::move(s), n, String{}); }
      Expr   get_or_default(SharedPtr<Style> s, ObjectStyleOptionName n) { return get_or_default(std::move(s), n, Expr{}); }
      
      Expr name() { return _name; }
      void unregister();
      bool register_as(Expr name);
      static SharedPtr<Stylesheet> find_registered(Expr name);
      
      static SharedPtr<Stylesheet> try_load(Expr expr);
      static Expr name_from_path(String filename);
      static String path_for_name(Expr name);
      void add(Expr expr);
      void reload();
      void reload(Expr expr);
      
      virtual void dynamic_updated() override {}
      
      void add_user(FrontEndObject *obj) const {
        assert(obj);
        if(users.add(obj->id())) {
          for(auto &other : used_stylesheets)
            other->add_user(obj);
        }
      }
      
      Hashset<FrontEndReference>::KeyEnum enum_users() const {
        return users.keys();
      }
    
    protected:
      virtual ObjectWithLimbo *next_in_limbo() final override { return _limbo_next; }
      virtual void next_in_limbo(ObjectWithLimbo *next) final override { RICHMATH_ASSERT(!_limbo_next); _limbo_next = next; }
    
    public:
      static SharedPtr<Stylesheet> Default;
      
      Hashtable<String, SharedPtr<Style> > styles;
    
    private:
      Hashset<SharedPtr<Stylesheet>> used_stylesheets;
      mutable Hashset<FrontEndReference> users;
      
    public:
      SharedPtr<Style> base;
      
    private:
      Expr _name;
      Expr _loaded_definition;
      ObjectWithLimbo *_limbo_next;
  };
};

#endif // RICHMATH__UTIL__STYLE_H__INCLUDED
