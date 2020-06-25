#ifndef RICHMATH__UTIL__STYLE_H__INCLUDED
#define RICHMATH__UTIL__STYLE_H__INCLUDED

#include <eval/observable.h>
#include <graphics/color.h>
#include <util/hashtable.h>
#include <util/pmath-extra.h>
#include <util/sharedptr.h>

namespace richmath {
  class Box;
  
  bool get_factor_of_scaled(Expr expr, double *value);
  
  enum class DefaultStyleOptionOffsets {
    None = 0,
    ButtonBox     = 0x00100000,
    TemplateBox   = 0x00200000,
    InputFieldBox = 0x00300000,
    PanelBox      = 0x00400000,
    FillBox       = 0x00500000
  };
  
  enum AutoBoolValues {
    AutoBoolFalse = 0,
    AutoBoolTrue = 1,
    AutoBoolAutomatic = 2
  };
  
  enum ColorStyleOptionName {
    Background = 0x00000,
    FontColor,
    SectionFrameColor
  };
  
  enum IntStyleOptionName {
    Antialiasing = 0x10000, // AutoBoolXXX
    AutoDelete,
    AutoNumberFormating,
    AutoSpacing,
    ContinuousAction,
    Editable,
    Enabled, // AutoBoolXXX
    Evaluatable,
    InternalHasModifiedWindowOption,
    InternalHasPendingDynamic,
    InternalRequiresChildResize,
    InternalUsesCurrentValueOfMouseOver,
    LineBreakWithin,
    Placeholder,
    ReturnCreatesNewSection,
    Saveable,
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
    Visible,
    WholeSectionGroupOpener,
    
    FontSlant,
    FontWeight,
    
    ButtonFrame, // -1 = Automatic,  other: ContainerType value
    ButtonSource, // ButtonSourceXXX
    WindowFrame, // WindowFrameType
    
    ButtonBoxDefaultEnabled      = Enabled      + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonFrame  = ButtonFrame  + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonSource = ButtonSource + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    FillBoxDefaultStripOnInput = StripOnInput + (int)DefaultStyleOptionOffsets::FillBox,
    
    InputFieldBoxDefaultContinuousAction = ContinuousAction + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultEnabled          = Enabled          + (int)DefaultStyleOptionOffsets::InputFieldBox,
    
    PanelBoxDefaultEnabled = Enabled + (int)DefaultStyleOptionOffsets::PanelBox
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
    WindowFrameNormal  = 0,
    WindowFramePalette = 1,
    WindowFrameDialog  = 2
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
    ImageSizeHorizontal, // > 0 or ImageSizeAutomatic or ImageSizeAll
    ImageSizeVertical,   // > 0 or ImageSizeAutomatic
    
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
    
    FillBoxDefaultFillBoxWeight = FillBoxWeight + (int)DefaultStyleOptionOffsets::FillBox
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
    FontFamilies,
    FontFeatures,
    MathFontFamily,
    UnknownOptions,
    
    ButtonBoxOptions,
    ButtonData,
    ButtonFunction,
    
    BoxRotation,
    BoxTransformation,
    PlotRange,
    BorderRadius,
    
    DefaultDuplicateSectionStyle,
    DefaultNewSectionStyle,
    DefaultReturnCreatedSectionStyle,
    
    DockedSections,
    DockedSectionsTop,
    DockedSectionsTopGlass,
    DockedSectionsBottom,
    DockedSectionsBottomGlass,
    
    FillBoxOptions,
    InputFieldBoxOptions,
    PanelBoxOptions,
    
    DisplayFunction,
    InterpretationFunction,
    SyntaxForm,
    Tooltip,
    TemplateBoxOptions,
    
    StyleDefinitions,
    InternalLastStyleDefinitions,
    GeneratedSectionStyles,
    
    SectionDingbat,
    Appearance,
    
    ButtonBoxDefaultAppearance               = Appearance             + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultBaselinePosition         = BaselinePosition       + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonData               = ButtonData             + (int)DefaultStyleOptionOffsets::ButtonBox,
    ButtonBoxDefaultButtonFunction           = ButtonFunction         + (int)DefaultStyleOptionOffsets::ButtonBox,
    
    InputFieldBoxDefaultAppearance           = Appearance             + (int)DefaultStyleOptionOffsets::InputFieldBox,
    InputFieldBoxDefaultBaselinePosition     = BaselinePosition       + (int)DefaultStyleOptionOffsets::InputFieldBox,
    
    PanelBoxDefaultAppearance           = Appearance             + (int)DefaultStyleOptionOffsets::PanelBox,
    PanelBoxDefaultBaselinePosition     = BaselinePosition       + (int)DefaultStyleOptionOffsets::PanelBox,
    
    TemplateBoxDefaultDisplayFunction        = DisplayFunction        + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultInterpretationFunction = InterpretationFunction + (int)DefaultStyleOptionOffsets::TemplateBox,
    TemplateBoxDefaultTooltip                = Tooltip                + (int)DefaultStyleOptionOffsets::TemplateBox
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
  
  enum StyleType {
    StyleTypeNone,
    
    StyleTypeBool,
    StyleTypeBoolAuto,
    StyleTypeColor,
    StyleTypeNumber,
    StyleTypeMargin,
    StyleTypeSize,
    StyleTypeString,
    StyleTypeAny,
    
    StyleTypeEnum,
    StyleTypeRuleSet
  };
  
  typedef union {
    int   int_value;
    float float_value;
  } IntFloatUnion;
  
  class Style: public Observable, public Shareable {
    friend class StyleImpl;
    public:
      Style();
      Style(Expr options);
      virtual ~Style();
      
      void clear();
      static void reset(SharedPtr<Style> &style, String base_style_name);
      
      void add_pmath(Expr options);
      
      void merge(SharedPtr<Style> other);
      static bool contains_inherited(Expr expr);
      static Expr merge_style_values(StyleOptionName n, Expr newer, Expr older);
      
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
      void set_pmath_by_unknown_key(Expr lhs, Expr rhs);
      
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
      
      bool update_dynamic(SharedPtr<Style> s, Box *parent);
      
      Color  get_with_base(SharedPtr<Style> s, ColorStyleOptionName  n);
      int    get_with_base(SharedPtr<Style> s, IntStyleOptionName    n);
      float  get_with_base(SharedPtr<Style> s, FloatStyleOptionName  n);
      String get_with_base(SharedPtr<Style> s, StringStyleOptionName n);
      Expr   get_with_base(SharedPtr<Style> s, ObjectStyleOptionName n);
      
      Expr get_pmath_with_base(SharedPtr<Style> s, StyleOptionName n);
      
      Expr name() { return _name; }
      void unregister();
      bool register_as(Expr name);
      static SharedPtr<Stylesheet> find_registered(Expr name);
      
      static SharedPtr<Stylesheet> try_load(Expr expr);
      static Expr name_from_path(String filename);
      void add(Expr expr);
      void reload();
      void reload(Expr expr);
      
      virtual void dynamic_updated() override {}
      
      void add_user(FrontEndObject *obj) {
        assert(obj);
        users.add(obj->id());
      }
      
      Hashset<FrontEndReference>::KeyEnum enum_users() const {
        return users.keys();
      }
      
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
  };
};

#endif // RICHMATH__UTIL__STYLE_H__INCLUDED
